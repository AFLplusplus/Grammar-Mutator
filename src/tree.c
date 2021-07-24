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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "tree.h"
#include "utils.h"

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

  // id
  node->id = 0;

  // rule id
  node->rule_id = 0;

  node->recursion_edge_size = 0;
  node->non_term_size = 0;

  // val buf
  if (node->val_buf) {

    free(node->val_buf);
    node->val_buf = NULL;
    node->val_size = 0;
    node->val_len = 0;

  }

  // parent node
  node->parent = NULL;

  // subnodes
  if (node->subnode_count != 0) {

    node_t *subnode = NULL;
    for (uint32_t i = 0; i < node->subnode_count; ++i) {

      subnode = node->subnodes[i];

      // `subnode` may be NULL due to parsing errors
      if (unlikely(!subnode)) continue;

      node_free(subnode);

    }

    node->subnode_count = 0;

  }

  if (node->subnodes) free(node->subnodes);

  free(node);

}

void node_free_only_self(node_t *node) {

  // Pretend we don't have any subnodes so that node_free() won't
  // call itself recursively
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

  node_t *new_node = node_create(node->id);

  // rule id
  new_node->rule_id = node->rule_id;

  new_node->recursion_edge_size = node->recursion_edge_size;
  new_node->non_term_size = node->non_term_size;

  // val
  node_set_val(new_node, node->val_buf, node->val_len);
  new_node->val_len = node->val_len;

  // subnodes
  if (node->subnode_count != 0) {

    node_init_subnodes(new_node, node->subnode_count);
    node_t *subnode = NULL;
    for (uint32_t i = 0; i < node->subnode_count; ++i) {

      subnode = node->subnodes[i];
      node_set_subnode(new_node, i, node_clone(subnode));

    }

  }

  return new_node;

}

bool node_equal(node_t *node_a, node_t *node_b) {

  if (node_a == node_b) return true;
  if (!node_a || !node_b) return false;
  if (node_a->id != node_b->id) return false;
  if (node_a->rule_id != node_b->rule_id) return false;
  if (node_a->val_len != node_b->val_len) return false;
  if (memcmp(node_a->val_buf, node_b->val_buf, node_a->val_len) != 0)
    return false;

  // Do not consider the parent node while comparing two nodes

  // subnodes
  if (node_a->subnode_count != node_b->subnode_count) return false;

  for (uint32_t i = 0; i < node_a->subnode_count; ++i) {

    if (!node_equal(node_a->subnodes[i], node_b->subnodes[i])) return false;

  }

  return true;

}

void node_get_size(node_t *node) {

  if (node == NULL) return;
  if (node->id == 0) {

    // terminal node
    node->non_term_size = 0;
    node->recursion_edge_size = 0;

    return;

  }

  node->non_term_size = 1;
  node->recursion_edge_size = 0;

  node_t *subnode = NULL;
  for (uint32_t i = 0; i < node->subnode_count; ++i) {

    subnode = node->subnodes[i];
    if (unlikely(!subnode)) continue;

    if (node->id == subnode->id) {

      // recursive link
      ++node->recursion_edge_size;

    }

    node_get_size(subnode);

    node->recursion_edge_size += subnode->recursion_edge_size;
    node->non_term_size += subnode->non_term_size;

  }

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
  if (node->non_term_size == 0) return NULL;

  size_t non_term_size = node->non_term_size;
  size_t prob = random_below(non_term_size);

  if (prob < 1) return node;
  prob -= 1;

  node_t *subnode = NULL;
  for (uint32_t i = 0; i < node->subnode_count; ++i) {

    subnode = node->subnodes[i];

    // `subnode` may be NULL due to parsing errors
    if (unlikely(!subnode)) continue;

    if (subnode->id == 0) continue;  // "0" means the terminal node

    if (prob < subnode->non_term_size)
      return node_pick_non_term_subnode(subnode);
    prob -= subnode->non_term_size;

  }

  // should not reach here
  return NULL;

}

edge_t node_pick_recursion_edge(node_t *node) {

  edge_t ret = {NULL, NULL, 0};
  if (!node) return ret;
  if (node->recursion_edge_size == 0) return ret;

  size_t recursion_edge_size = node->recursion_edge_size;
  size_t prob = random_below(recursion_edge_size);

  node_t *subnode = NULL;
  for (uint32_t i = 0; i < node->subnode_count; ++i) {

    subnode = node->subnodes[i];

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
    if (prob < subnode->recursion_edge_size)
      return node_pick_recursion_edge(subnode);

    prob -= subnode->recursion_edge_size;

  }

  // should not reach here
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

void _node_to_buf(tree_t *tree, node_t *node) {

  if (!tree || !node) return;

  // dump `val` if this is a leaf node
  if (node->subnode_count == 0) {

    if (node->val_len == 0) return;

    size_t   data_len = tree->data_len;
    uint8_t *data_buf =
        maybe_grow(BUF_PARAMS(tree, data), data_len + node->val_len);
    if (!data_buf) {

      perror("tree output buffer allocation (maybe_grow)");
      return;

    }

    memcpy(data_buf + data_len, node->val_buf, node->val_len);
    tree->data_len += node->val_len;

    return;

  }

  // subnodes
  node_t *subnode = NULL;
  for (uint32_t i = 0; i < node->subnode_count; ++i) {

    subnode = node->subnodes[i];
    _node_to_buf(tree, subnode);

  }

}

void _node_get_recursion_edges(tree_t *tree, node_t *node) {

  if (!tree || !node) return;
  if (node->subnode_count == 0) return;

  // subnodes
  node_t *subnode = NULL;
  for (uint32_t i = 0; i < node->subnode_count; ++i) {

    subnode = node->subnodes[i];

    // `subnode` may be NULL due to parsing errors
    if (unlikely(!subnode)) continue;

    if (node->id == subnode->id) {

      edge_t *edge = malloc(sizeof(edge_t));
      edge->parent = node;
      edge->subnode = subnode;
      edge->subnode_offset = i;
      list_append(tree->recursion_edge_list, edge);

    }

    _node_get_recursion_edges(tree, subnode);

  }

}

void _node_get_non_terminal_nodes(tree_t *tree, node_t *node) {

  if (!tree || !node) return;
  if (node->id == 0) return;

  list_append(tree->non_terminal_node_list, node);

  // subnodes
  node_t *subnode = NULL;
  for (uint32_t i = 0; i < node->subnode_count; ++i) {

    subnode = node->subnodes[i];
    _node_get_non_terminal_nodes(tree, subnode);

  }

}

void _node_serialize(tree_t *tree, node_t *node) {

  if (!tree || !node) return;

  // allocate or update the buffer
  size_t len = sizeof(node->id) + sizeof(node->rule_id) +
               sizeof(node->subnode_count) + sizeof(node->val_len) +
               node->val_len;
  size_t   ser_len = tree->ser_len;
  uint8_t *ser_buf = maybe_grow(BUF_PARAMS(tree, ser), ser_len + len);
  if (!ser_buf) {

    perror("tree serialization buffer allocation (maybe_grow)");
    return;

  }

  // save `id`
  memcpy(ser_buf + ser_len, &(node->id), sizeof(node->id));
  ser_len += sizeof(node->id);

  // save `rule_id`
  memcpy(ser_buf + ser_len, &(node->rule_id), sizeof(node->rule_id));
  ser_len += sizeof(node->rule_id);

  // save `subnode_count`
  memcpy(ser_buf + ser_len, &(node->subnode_count),
         sizeof(node->subnode_count));
  ser_len += sizeof(node->subnode_count);

  // save `val`
  // - save `val_len`
  memcpy(ser_buf + ser_len, &(node->val_len), sizeof(node->val_len));
  ser_len += sizeof(node->val_len);

  // - save `val_buf`
  memcpy(ser_buf + ser_len, node->val_buf, node->val_len);
  ser_len += node->val_len;

  tree->ser_len = ser_len;

  // subnodes
  node_t *subnode = NULL;
  for (uint32_t i = 0; i < node->subnode_count; ++i) {

    subnode = node->subnodes[i];
    _node_serialize(tree, subnode);

  }

}

node_t *_node_deserialize(const uint8_t *data_buf, size_t data_size,
                          size_t *consumed_size) {

  if (!data_buf) return NULL;

  node_t *node = node_create(0);
  size_t  min_len = sizeof(node->id) + sizeof(node->rule_id) +
                   sizeof(node->subnode_count) + sizeof(node->val_len);
  if (data_size - (*consumed_size) < min_len) {

    // data is not enough for a node
    node_free(node);
    return NULL;

  }

  size_t ser_len = *consumed_size;

  // `id`
  memcpy(&(node->id), data_buf + ser_len, sizeof(node->id));
  ser_len += sizeof(node->id);

  // `rule_id`
  memcpy(&(node->rule_id), data_buf + ser_len, sizeof(node->rule_id));
  ser_len += sizeof(node->rule_id);

  // `subnode_count`
  memcpy(&(node->subnode_count), data_buf + ser_len,
         sizeof(node->subnode_count));
  ser_len += sizeof(node->subnode_count);

  // `val`
  // - `val_len`
  memcpy(&(node->val_len), data_buf + ser_len, sizeof(node->val_len));
  ser_len += sizeof(node->val_len);

  // - `val_buf`
  node_set_val(node, (data_buf + ser_len), node->val_len);
  ser_len += node->val_len;

  *consumed_size = ser_len;

  if (!node->subnode_count) return node;

  // subnodes
  node_init_subnodes(node, node->subnode_count);

  node_t *subnode = NULL;
  for (uint32_t i = 0; i < node->subnode_count; ++i) {

    subnode = _node_deserialize(data_buf, data_size, consumed_size);
    if (unlikely(!subnode)) {

      // unlikely reach here
      node_free(node);
      return NULL;

    }

    node_set_subnode(node, i, subnode);

  }

  return node;

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

void tree_to_buf(tree_t *tree) {

  if (!tree) return;

  maybe_grow(BUF_PARAMS(tree, data), TREE_BUF_PREALLOC_SIZE);
  tree->data_len = 0;

  _node_to_buf(tree, tree->root);

}

void tree_serialize(tree_t *tree) {

  if (!tree) return;

  maybe_grow(BUF_PARAMS(tree, ser), TREE_BUF_PREALLOC_SIZE);
  tree->ser_len = 0;

  _node_serialize(tree, tree->root);

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
  node_get_size(tree->root);
  return tree->root->non_term_size;

}

void tree_get_recursion_edges(tree_t *tree) {

  if (!tree) return;

  if (tree->recursion_edge_list)
    list_free_with_data_free_func(tree->recursion_edge_list, free);
  tree->recursion_edge_list = list_create();

  _node_get_recursion_edges(tree, tree->root);

}

void tree_get_non_terminal_nodes(tree_t *tree) {

  if (!tree) return;

  if (tree->non_terminal_node_list) list_free(tree->non_terminal_node_list);
  tree->non_terminal_node_list = list_create();

  _node_get_non_terminal_nodes(tree, tree->root);

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

tree_t *load_tree_from_test_case(const char *filename) {

  tree_t *tree = NULL;

  // Read the corresponding test case from file
  int fd = open(filename, O_RDONLY);
  if (unlikely(fd < 0)) return NULL;  // may not exist

  struct stat info;
  if (unlikely(fstat(fd, &info) != 0)) {

    // error, no file info
    perror("Cannot get file information");
    return NULL;

  }

  size_t   file_size = info.st_size;
  uint8_t *buf =
      (uint8_t *)mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (unlikely(buf == MAP_FAILED)) {

    perror("Cannot map the test case file to the memory");
    return NULL;

  }

  close(fd);

  // Deserialize the data to recover the tree
  tree = tree_from_buf(buf, file_size);
  munmap(buf, file_size);
  if (unlikely(!tree)) {

    // error, cannot parse the data
    return NULL;

  }

  return tree;

}

void write_tree_to_file(tree_t *tree, const char *filename) {

  int fd, ret;

  // Serialize the tree
  tree_serialize(tree);

  // Open the file
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (unlikely(fd < 0)) {

    perror("Unable to create the file (write_tree_to_file)");
    return;

  }

  // Write the data
  ret = write(fd, tree->ser_buf, tree->ser_len);
  if (unlikely(ret < 0)) {

    perror("Unable to write (write_tree_to_file)");
    return;

  }

  if (unlikely((size_t)ret != tree->ser_len)) {

    perror("Short write to tree file (write_tree_to_file)");
    return;

  }

  close(fd);

}

void dump_tree_to_test_case(tree_t *tree, const char *filename) {

  int fd, ret;

  // Unparse the tree
  tree_to_buf(tree);

  // Open the file
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (unlikely(fd < 0)) {

    perror("Unable to create the file (dump_tree_to_test_case)");
    exit(EXIT_FAILURE);

  }

  // Write the data
  ret = write(fd, tree->data_buf, tree->data_len);
  if (unlikely(ret < 0)) {

    perror("Unable to write (write_tree_to_file)");
    return;

  }

  if (unlikely((size_t)ret != tree->data_len)) {

    perror("Short write to tree file (dump_tree_to_test_case)");
    return;

  }

  close(fd);

}
