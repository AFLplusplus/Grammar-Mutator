/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Written by Shengtuo Hu

   Copyright 2020 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "tree.h"
#include "utils.h"
#include "node_stack.h"

#define TREE_BUF_PREALLOC_SIZE (64)

node_t *node_create(uint32_t id) {

  node_t *node = calloc(1, sizeof(node_t));
  if (!node) {

    perror("node_create (calloc)");
    return NULL;

  }

  node->id = id;
  node->recursion_edge_size = 0;
  if (id != 0) {  // "0" means the terminal node
    node->non_term_size = 1;

  }

  return node;

}

node_t *node_create_with_rule_id(uint32_t id, uint32_t rule_id) {

  node_t *node = node_create(id);
  node->rule_id = rule_id;
  return node;

}

node_t *node_create_with_val(uint32_t id, const void *val_buf, size_t val_len) {

  node_t *node = node_create_with_rule_id(id, 0);

  if (val_buf) node_set_val(node, val_buf, val_len);

  return node;

}

void node_init_subnodes(node_t *node, size_t n) {

  if (node == NULL) return;
  if (node->id == 0) return;  // terminal node should not have subnodes

  if (n == 0) {

    // clear subnode array
    if (node->subnodes) {

      free(node->subnodes);
      node->subnodes = NULL;

    }

    node->subnode_count = 0;
    return;

  }

  // TODO: do we need to free subnodes before reallocation?
  if (node->subnodes) {

    node->subnodes = realloc(node->subnodes, n * sizeof(node_t *));

  } else {

    node->subnodes = calloc(n, sizeof(node_t *));

  }

  if (!node->subnodes) {

    perror("node_init_subnodes (realloc or calloc)");
    return;

  }

  node->subnode_count = n;

}

void node_free(node_t *node) {

  if (!node) return;

  // This function cannot report an error. If the stack cannot grow, the
  // function leaks the subtrees that it cannot push. A leak is better than a
  // crash here.
  node_stack_t stack = {0};
  if (!node_stack_push(&stack, node, 0, 0)) {

    node->subnode_count = 0;
    if (node->val_buf) free(node->val_buf);
    if (node->subnodes) free(node->subnodes);
    free(node);
    return;

  }

  while (stack.len) {

    node = node_stack_pop(&stack).node;

    // `subnode` may be NULL due to parsing errors
    for (uint32_t i = 0; i < node->subnode_count; ++i) {

      // On a push failure, the subtree of `subnode` leaks.
      if (likely(node->subnodes[i]))
        (void)node_stack_push(&stack, node->subnodes[i], i, 0);

    }

    node->id = 0;
    node->rule_id = 0;
    node->recursion_edge_size = 0;
    node->non_term_size = 0;
    node->parent = NULL;
    node->subnode_count = 0;
    if (node->val_buf) free(node->val_buf);
    if (node->subnodes) free(node->subnodes);
    free(node);

  }

  node_stack_free(&stack);

}

void node_free_only_self(node_t *node) {

  // Pretend we don't have any subnodes so that node_free() frees only
  // this node and does not walk into its subtree
  node->subnode_count = 0;
  node_free(node);

}

void node_set_val(node_t *node, const void *val_buf, size_t val_len) {

  if (node == NULL) return;
  // TODO: the following line is necessary, but we need to update the polled
  //  string
  // if (node->id != 0) return;  // non-terminal node should not have a value
  if (val_len == 0) return;
  if (!val_buf) return;

  uint8_t *buf = maybe_grow(BUF_PARAMS(node, val), val_len);
  if (!buf) {

    perror("node_set_val (maybe_grow)");
    return;

  }

  node->val_len = val_len;
  memcpy(buf, val_buf, val_len);

}

void node_set_subnode(node_t *node, size_t i, node_t *subnode) {

  if (node == NULL) return;
  if (node->id == 0) return;  // terminal node should not have subnodes
  if (i >= node->subnode_count) return;
  if (!node->subnodes) return;

  node->subnodes[i] = subnode;
  if (subnode) subnode->parent = node;  // set the parent

  // Note that, this function does not update the `recursion_edge_size` and
  // `non_term_size`

}

node_t *node_clone(node_t *node) {

  if (!node) return NULL;

  node_t *root = node_create(node->id);
  if (!root) return NULL;
  node_stack_t stack = {0};
  if (!node_stack_push(&stack, node, 0, (size_t)root)) {

    node_free(root);
    return NULL;

  }

  while (stack.len) {

    node_frame_t f = node_stack_pop(&stack);
    node_t      *src = f.node;
    node_t      *dst = (node_t *)f.aux;

    dst->rule_id = src->rule_id;
    dst->recursion_edge_size = src->recursion_edge_size;
    dst->non_term_size = src->non_term_size;

    node_set_val(dst, src->val_buf, src->val_len);
    dst->val_len = src->val_len;

    if (src->subnode_count != 0) {

      node_init_subnodes(dst, src->subnode_count);
      // A NULL subnode stays NULL
      for (uint32_t i = src->subnode_count; i-- > 0;) {

        if (!src->subnodes[i]) continue;
        node_t *child = node_create(src->subnodes[i]->id);
        if (!child) goto fail;
        node_set_subnode(dst, i, child);
        if (!node_stack_push(&stack, src->subnodes[i], i, (size_t)child))
          goto fail;

      }

    }

  }

  node_stack_free(&stack);
  return root;

fail:
  // Free the partial copy. Its children are linked, so it is a valid tree.
  node_stack_free(&stack);
  node_free(root);
  return NULL;

}

bool node_equal(node_t *node_a, node_t *node_b) {

  if (node_a == node_b) return true;
  if (!node_a || !node_b) return false;

  // frames hold the first node; `aux` holds the second
  // If a push fails, the function answers "not equal".
  node_stack_t stack = {0};
  if (!node_stack_push(&stack, node_a, 0, (size_t)node_b)) return false;
  bool equal = true;

  while (equal && stack.len) {

    node_frame_t f = node_stack_pop(&stack);
    node_t      *a = f.node;
    node_t      *b = (node_t *)f.aux;

    if (a == b) continue;
    if (!a || !b || a->id != b->id || a->rule_id != b->rule_id ||
        a->val_len != b->val_len ||
        memcmp(a->val_buf, b->val_buf, a->val_len) != 0 ||
        a->subnode_count != b->subnode_count) {

      equal = false;
      break;

    }

    // Do not consider the parent node while comparing two nodes
    for (uint32_t i = 0; i < a->subnode_count; ++i) {

      if (!node_stack_push(&stack, a->subnodes[i], i, (size_t)b->subnodes[i])) {

        equal = false;
        break;

      }

    }

  }

  node_stack_free(&stack);
  return equal;

}

bool node_get_size(node_t *node) {

  if (node == NULL) return true;

  // Collect the nodes in pre-order. Their reverse has every child before its
  // parent. A terminal node (id 0) is a boundary.
  node_stack_t stack = {0};
  node_stack_t order = {0};
  node_t      *top = node;
  bool         ok = node_stack_push(&stack, node, 0, 0);

  while (ok && stack.len) {

    node_frame_t f = node_stack_pop(&stack);
    ok = node_stack_push(&order, f.node, 0, 0);
    if (!ok) break;
    if (f.node->id == 0) continue;
    ok = node_stack_push_children(&stack, f.node, 0);

  }

  if (!ok) {

    // Set the sizes of the top node to 0. Then the picks find no node, and the
    // random and splicing mutations return NULL. afl_custom_fuzz_count calls
    // this function again and plans no mutation if it fails.
    top->non_term_size = 0;
    top->recursion_edge_size = 0;
    node_stack_free(&stack);
    node_stack_free(&order);
    return false;

  }

  while (order.len) {

    node = node_stack_pop(&order).node;
    if (node->id == 0) {

      node->non_term_size = 0;
      node->recursion_edge_size = 0;
      continue;

    }

    node->non_term_size = 1;
    node->recursion_edge_size = 0;

    for (uint32_t i = 0; i < node->subnode_count; ++i) {

      node_t *subnode = node->subnodes[i];
      if (unlikely(!subnode)) continue;

      if (node->id == subnode->id) ++node->recursion_edge_size;
      node->recursion_edge_size += subnode->recursion_edge_size;
      node->non_term_size += subnode->non_term_size;

    }

  }

  node_stack_free(&stack);
  node_stack_free(&order);
  return true;

}

bool node_replace_subnode(node_t *root, node_t *subnode, node_t *new_subnode) {

  if (!root || !subnode || !new_subnode) return false;
  if (subnode->id != new_subnode->id) return false;
  if (root != subnode->parent) return false;

  node_t *cur = NULL;
  for (uint32_t i = 0; i < root->subnode_count; ++i) {

    cur = root->subnodes[i];

    if (cur == subnode) {

      root->subnodes[i] = new_subnode;
      new_subnode->parent = root;

      // Detach `subnode` from the parent
      cur->parent = NULL;

      return true;

    }

  }

  return false;

}

node_t *node_pick_non_term_subnode(node_t *node) {

  if (!node) return NULL;

  // Descend one level per pass. Each level draws a new random number.
  while (1) {

    if (node->non_term_size == 0) return NULL;

    size_t prob = random_below(node->non_term_size);
    if (prob < 1) return node;
    prob -= 1;

    node_t *next = NULL;
    for (uint32_t i = 0; i < node->subnode_count; ++i) {

      node_t *subnode = node->subnodes[i];

      // `subnode` may be NULL due to parsing errors
      if (unlikely(!subnode)) continue;

      if (subnode->id == 0) continue;  // "0" means the terminal node

      if (prob < subnode->non_term_size) {

        next = subnode;
        break;

      }

      prob -= subnode->non_term_size;

    }

    // should not reach here when `next` is NULL
    if (!next) return NULL;
    node = next;

  }

}

edge_t node_pick_recursion_edge(node_t *node) {

  edge_t ret = {NULL, NULL, 0};

  while (node) {

    if (node->recursion_edge_size == 0) return ret;

    size_t  prob = random_below(node->recursion_edge_size);
    node_t *next = NULL;

    for (uint32_t i = 0; i < node->subnode_count; ++i) {

      node_t *subnode = node->subnodes[i];

      // `subnode` may be NULL due to parsing errors
      if (unlikely(!subnode)) continue;

      // "node -> subnode" is a recursion edge
      if (node->id == subnode->id) {

        if (prob < 1) {

          // select this edge
          ret.parent = node;
          ret.subnode = subnode;
          ret.subnode_offset = i;
          return ret;

        }

        prob -= 1;

      }

      // pick from this subnode
      if (prob < subnode->recursion_edge_size) {

        next = subnode;
        break;

      }

      prob -= subnode->recursion_edge_size;

    }

    // should not reach here when `next` is NULL
    node = next;

  }

  return ret;

}

edge_t node_get_parent_edge(node_t *node) {

  edge_t ret = {NULL, NULL, 0};
  if (!node) return ret;
  if (!node->parent) return ret;

  ret.parent = node->parent;
  ret.subnode = node;

  node_t *parent = node->parent;
  for (uint32_t i = 0; i < parent->subnode_count; ++i) {

    if (node == parent->subnodes[i]) {

      ret.subnode_offset = i;
      break;

    }

  }

  return ret;

}

bool _node_to_buf(tree_t *tree, node_t *node) {

  if (!tree || !node) return true;

  node_stack_t stack = {0};
  bool         ok = node_stack_push(&stack, node, 0, 0);

  while (ok && stack.len) {

    node = node_stack_pop(&stack).node;

    // dump `val` if this is a leaf node
    if (node->subnode_count == 0) {

      if (node->val_len == 0) continue;

      size_t   data_len = tree->data_len;
      uint8_t *data_buf =
          maybe_grow(BUF_PARAMS(tree, data), data_len + node->val_len);
      if (!data_buf) {

        perror("tree output buffer allocation (maybe_grow)");
        ok = false;
        break;

      }

      memcpy(data_buf + data_len, node->val_buf, node->val_len);
      tree->data_len += node->val_len;
      continue;

    }

    ok = node_stack_push_children(&stack, node, 0);

  }

  node_stack_free(&stack);
  return ok;

}

bool _node_get_recursion_edges(tree_t *tree, node_t *node) {

  if (!tree || !node) return true;

  // Visit the nodes in pre-order; the edge of a node comes with the node.
  // If an allocation fails, the walk stops and returns false. The list is then
  // short, and the caller must not use it.
  node_stack_t stack = {0};
  bool         ok = node_stack_push(&stack, node, 0, 0);
  node_t      *root = node;

  while (ok && stack.len) {

    node_frame_t f = node_stack_pop(&stack);
    node = f.node;

    if (node != root && node->parent && node->parent->id == node->id) {

      edge_t *edge = malloc(sizeof(edge_t));
      if (!edge || !list_append(tree->recursion_edge_list, edge)) {

        free(edge);
        ok = false;
        break;

      }

      edge->parent = node->parent;
      edge->subnode = node;
      edge->subnode_offset = f.idx;

    }

    ok = node_stack_push_children(&stack, node, 0);

  }

  node_stack_free(&stack);
  return ok;

}

bool _node_get_non_terminal_nodes(tree_t *tree, node_t *node) {

  if (!tree || !node) return true;

  // If an allocation fails, the walk stops and returns false. The list is then
  // short, and the caller must not use it.
  node_stack_t stack = {0};
  bool         ok = node_stack_push(&stack, node, 0, 0);

  while (ok && stack.len) {

    node = node_stack_pop(&stack).node;
    if (node->id == 0) continue;

    ok = list_append(tree->non_terminal_node_list, node) != NULL &&
         node_stack_push_children(&stack, node, 0);

  }

  node_stack_free(&stack);
  return ok;

}

bool _node_serialize(tree_t *tree, node_t *node) {

  if (!tree || !node) return true;

  node_stack_t stack = {0};
  bool         ok = node_stack_push(&stack, node, 0, 0);

  while (ok && stack.len) {

    node = node_stack_pop(&stack).node;

    // allocate or update the buffer
    size_t   len = sizeof(node->id) + sizeof(node->rule_id) +
                   sizeof(node->subnode_count) + sizeof(node->val_len) +
                   node->val_len;
    size_t   ser_len = tree->ser_len;
    uint8_t *ser_buf = maybe_grow(BUF_PARAMS(tree, ser), ser_len + len);
    if (!ser_buf) {

      perror("tree serialization buffer allocation (maybe_grow)");
      ok = false;
      break;

    }

    memcpy(ser_buf + ser_len, &(node->id), sizeof(node->id));
    ser_len += sizeof(node->id);
    memcpy(ser_buf + ser_len, &(node->rule_id), sizeof(node->rule_id));
    ser_len += sizeof(node->rule_id);
    memcpy(ser_buf + ser_len, &(node->subnode_count),
           sizeof(node->subnode_count));
    ser_len += sizeof(node->subnode_count);
    memcpy(ser_buf + ser_len, &(node->val_len), sizeof(node->val_len));
    ser_len += sizeof(node->val_len);
    memcpy(ser_buf + ser_len, node->val_buf, node->val_len);
    ser_len += node->val_len;

    tree->ser_len = ser_len;

    ok = node_stack_push_children(&stack, node, 0);

  }

  node_stack_free(&stack);
  return ok;

}

// Read one node header and its value. Return NULL if the data is bad. The
// subnodes are not read.
static node_t *_node_read(const uint8_t *data_buf, size_t data_size,
                          size_t *consumed_size) {

  node_t *node = node_create(0);
  size_t  min_len = sizeof(node->id) + sizeof(node->rule_id) +
                    sizeof(node->subnode_count) + sizeof(node->val_len);
  if (data_size - (*consumed_size) < min_len) {

    // data is not enough for a node
    node_free(node);
    return NULL;

  }

  size_t ser_len = *consumed_size;

  memcpy(&(node->id), data_buf + ser_len, sizeof(node->id));
  ser_len += sizeof(node->id);
  memcpy(&(node->rule_id), data_buf + ser_len, sizeof(node->rule_id));
  ser_len += sizeof(node->rule_id);
  memcpy(&(node->subnode_count), data_buf + ser_len,
         sizeof(node->subnode_count));
  ser_len += sizeof(node->subnode_count);
  memcpy(&(node->val_len), data_buf + ser_len, sizeof(node->val_len));
  ser_len += sizeof(node->val_len);

  // A corrupt length must not read past the data or allocate too much. Each
  // subnode needs at least `min_len` bytes. A leaf (id 0) has no subnodes.
  if ((node->id == 0 && node->subnode_count) ||
      node->val_len > data_size - ser_len ||
      node->subnode_count > (data_size - ser_len - node->val_len) / min_len) {

    node->val_len = 0;
    node->subnode_count = 0;
    node_free(node);
    return NULL;

  }

  node_set_val(node, (data_buf + ser_len), node->val_len);
  ser_len += node->val_len;

  *consumed_size = ser_len;

  if (node->subnode_count) node_init_subnodes(node, node->subnode_count);

  return node;

}

node_t *_node_deserialize(const uint8_t *data_buf, size_t data_size,
                          size_t *consumed_size) {

  if (!data_buf) return NULL;

  node_t *root = _node_read(data_buf, data_size, consumed_size);
  if (!root) return NULL;

  // frames: `node` is the parent, `idx` is the next child to read
  node_stack_t stack = {0};
  if (root->subnode_count) node_stack_push(&stack, root, 0, 0);

  while (stack.len) {

    node_frame_t *top = &stack.items[stack.len - 1];
    if (top->idx == top->node->subnode_count) {

      --stack.len;
      continue;

    }

    node_t *subnode = _node_read(data_buf, data_size, consumed_size);
    if (unlikely(!subnode)) {

      // unlikely reach here; the unread subnodes are NULL
      node_free(root);
      node_stack_free(&stack);
      return NULL;

    }

    node_set_subnode(top->node, top->idx++, subnode);
    if (subnode->subnode_count) node_stack_push(&stack, subnode, 0, 0);

  }

  node_stack_free(&stack);
  return root;

}

inline tree_t *tree_create() {

  return calloc(1, sizeof(tree_t));

}

void tree_free(tree_t *tree) {

  if (!tree) return;

  // root node
  node_free(tree->root);
  tree->root = NULL;

  // data buf
  if (tree->data_buf) {

    free(tree->data_buf);
    tree->data_buf = NULL;
    tree->data_size = 0;
    tree->data_len = 0;

  }

  // ser buf
  if (tree->ser_buf) {

    free(tree->ser_buf);
    tree->ser_buf = NULL;
    tree->ser_size = 0;
    tree->ser_len = 0;

  }

  // non-ternimal node list
  if (tree->non_terminal_node_list) {

    // no need to free the data for each node
    list_free(tree->non_terminal_node_list);
    tree->non_terminal_node_list = NULL;

  }

  // recursion edge list
  if (tree->recursion_edge_list) {

    list_free_with_data_free_func(tree->recursion_edge_list, free);
    tree->recursion_edge_list = NULL;

  }

  free(tree);

}

bool tree_to_buf(tree_t *tree) {

  if (!tree) return false;

  maybe_grow(BUF_PARAMS(tree, data), TREE_BUF_PREALLOC_SIZE);
  tree->data_len = 0;

  if (!_node_to_buf(tree, tree->root)) {

    // Do not leave a short buffer that looks complete.
    tree->data_len = 0;
    return false;

  }

  return true;

}

bool tree_serialize(tree_t *tree) {

  if (!tree) return false;

  maybe_grow(BUF_PARAMS(tree, ser), TREE_BUF_PREALLOC_SIZE);
  tree->ser_len = 0;

  if (!_node_serialize(tree, tree->root)) {

    tree->ser_len = 0;
    return false;

  }

  return true;

}

tree_t *tree_deserialize(const uint8_t *data_buf, size_t data_size) {

  size_t  consumed_size = 0;
  node_t *root = _node_deserialize(data_buf, data_size, &consumed_size);
  if (!root) return NULL;
  if (consumed_size > data_size) {

    node_free(root);
    return NULL;

  }

  tree_t *tree = tree_create();
  tree->root = root;
  return tree;

}

tree_t *tree_clone(tree_t *tree) {

  tree_t *new_tree = tree_create();
  new_tree->root = node_clone(tree->root);
  if (tree->root && !new_tree->root) {

    tree_free(new_tree);
    return NULL;

  }

  // Do not clone the data buffer, as the cloned tree is likely for mutations
  new_tree->data_buf = NULL;
  new_tree->data_size = 0;
  new_tree->data_len = 0;

  return new_tree;

}

inline bool tree_equal(tree_t *tree_a, tree_t *tree_b) {

  if (tree_a == tree_b) return true;
  if (!tree_a || !tree_b) return false;
  return node_equal(tree_a->root, tree_b->root);

}

inline size_t tree_get_size(tree_t *tree) {

  if (tree->root->id == 0) return 0;
  if (!node_get_size(tree->root)) return 0;
  return tree->root->non_term_size;

}

bool tree_get_recursion_edges(tree_t *tree) {

  if (!tree) return false;

  if (tree->recursion_edge_list)
    list_free_with_data_free_func(tree->recursion_edge_list, free);
  tree->recursion_edge_list = list_create();
  if (!tree->recursion_edge_list) return false;

  if (_node_get_recursion_edges(tree, tree->root)) return true;

  // Leave an empty list, so that no caller uses a short one
  list_free_with_data_free_func(tree->recursion_edge_list, free);
  tree->recursion_edge_list = list_create();
  return false;

}

bool tree_get_non_terminal_nodes(tree_t *tree) {

  if (!tree) return false;

  if (tree->non_terminal_node_list) list_free(tree->non_terminal_node_list);
  tree->non_terminal_node_list = list_create();
  if (!tree->non_terminal_node_list) return false;

  if (_node_get_non_terminal_nodes(tree, tree->root)) return true;

  // Leave an empty list, so that no caller uses a short one
  list_free(tree->non_terminal_node_list);
  tree->non_terminal_node_list = list_create();
  return false;

}

tree_t *read_tree_from_file(const char *filename) {

  tree_t *tree = NULL;

  // Read the corresponding serialized tree from file
  int fd = open(filename, O_RDONLY);
  if (unlikely(fd < 0)) return NULL;  // may not exist

  struct stat info;
  if (unlikely(fstat(fd, &info) != 0)) {

    // error, no file info
    perror("Cannot get file information");
    return NULL;

  }

  size_t   tree_file_size = info.st_size;
  uint8_t *tree_buf = (uint8_t *)mmap(0, tree_file_size, PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE, fd, 0);
  if (unlikely(tree_buf == MAP_FAILED)) {

    perror("Cannot map the tree file to the memory");
    return NULL;

  }

  close(fd);

  // Deserialize the data to recover the tree
  tree = tree_deserialize(tree_buf, tree_file_size);
  munmap(tree_buf, tree_file_size);
  if (unlikely(!tree)) {

    perror("Cannot deserialize the data");
    return NULL;

  }

  return tree;

}

tree_t *tree_from_buf(const uint8_t *data_buf, size_t data_size) {

  return tree_parse(data_buf, data_size, NULL, NULL);

}

tree_t *load_tree_from_test_case(const char *filename) {

  return load_tree_from_test_case_ex(filename, NULL, NULL);

}

/* If the code cannot open or read the file, it returns NULL and leaves
  "*status" as it is. The caller sets "*status" to PARSE_OK before the call.
  So NULL with PARSE_OK means "not read". The caller must not remember such
  an entry as rejected. */
tree_t *load_tree_from_test_case_ex(const char     *filename,
                                    parse_budget_t *budget,
                                    parse_status_t *status) {

  tree_t *tree = NULL;

  // Read the corresponding test case from file
  int fd = open(filename, O_RDONLY);
  if (unlikely(fd < 0)) return NULL;  // may not exist

  struct stat info;
  if (unlikely(fstat(fd, &info) != 0)) {

    // error, no file info
    perror("Cannot get file information");
    close(fd);
    return NULL;

  }

  size_t file_size = info.st_size;
  if (file_size == 0) {

    // Do not map an empty file. Parse an empty buffer.
    close(fd);
    static const uint8_t empty = 0;
    return tree_parse(&empty, 0, budget, status);

  }

  uint8_t *buf =
      (uint8_t *)mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (unlikely(buf == MAP_FAILED)) {

    perror("Cannot map the test case file to the memory");
    close(fd);
    return NULL;

  }

  close(fd);

  // Parse the data to build the tree
  tree = tree_parse(buf, file_size, budget, status);
  munmap(buf, file_size);
  return tree;

}

// Write `len` bytes of `buf` to a new file. On an error, print a message,
// remove the incomplete file, and return false. `who` names the caller in the
// messages.
static bool write_whole_file(const char *filename, const uint8_t *buf,
                             size_t len, const char *who) {

  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (unlikely(fd < 0)) {

    fprintf(stderr, "Unable to create the file %s (%s): %s\n", filename, who,
            strerror(errno));
    return false;

  }

  ssize_t ret = write(fd, buf, len);
  int     err = errno;
  bool    ok = ret >= 0 && (size_t)ret == len;
  if (close(fd) != 0 && ok) {

    err = errno;
    ok = false;

  }

  if (unlikely(!ok)) {

    if (ret >= 0 && (size_t)ret != len)
      fprintf(stderr, "Short write to the file %s (%s)\n", filename, who);
    else
      fprintf(stderr, "Unable to write the file %s (%s): %s\n", filename, who,
              strerror(err));
    unlink(filename);
    return false;

  }

  return true;

}

bool write_tree_to_file(tree_t *tree, const char *filename) {

  // Serialize the tree. Without memory for it, leave no tree file.
  if (!tree_serialize(tree)) {

    fprintf(
        stderr,
        "Unable to serialize the tree: out of memory (write_tree_to_file)\n");
    unlink(filename);  // no old file of another tree stays
    return false;

  }

  return write_whole_file(filename, tree->ser_buf, tree->ser_len,
                          "write_tree_to_file");

}

bool dump_tree_to_test_case(tree_t *tree, const char *filename) {

  // Unparse the tree. Without memory for it, leave no test case file.
  if (!tree_to_buf(tree)) {

    fprintf(stderr,
            "Unable to unparse the tree: out of memory "
            "(dump_tree_to_test_case)\n");
    unlink(filename);  // no old file of another tree stays
    return false;

  }

  return write_whole_file(filename, tree->data_buf, tree->data_len,
                          "dump_tree_to_test_case");

}
