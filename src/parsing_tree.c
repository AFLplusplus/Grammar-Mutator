#include "parsing_tree.h"

#define TREE_BUF_PREALLOC_SIZE (64)


inline node_t *node_create(uint32_t id) {
  node_t *node = calloc(1, sizeof(node_t));
  node->id = id;
  return node;
}

node_t *node_create_with_val(uint32_t id, const void *val_buf, size_t val_len) {
  node_t *node = node_create(id);

  node_set_val(node, val_buf, val_len);

  return node;
}

void node_free(node_t *node) {
  // val buf
  if (node->val_buf) {
    free(node->val_buf);
    node->val_buf = NULL;
    node->val_size = 0;
    node->val_len = 0;
  }

  // subnodes
  if (node->subnode_count != 0) {
    node_t *subnode = node->subnodes; // subnode linked list
    node_t *tmp = NULL;
    while (subnode) {
      tmp = subnode->next;
      node_free(subnode);
      subnode = tmp;
    }
    node->subnodes = NULL;
    node->subnode_last = NULL;
    node->subnode_count = 0;
  }

  free(node);
}

void node_set_val(node_t *node, const void *val_buf, size_t val_len) {
  if (val_len == 0) return;

  uint8_t *buf = maybe_grow(BUF_PARAMS(node, val), val_len);
  if (!buf) {
    perror("node_set_val (maybe_grow)");
    return;
  }

  node->val_len = val_len;
  memcpy(buf, val_buf, val_len);
}

inline void node_append_subnode(node_t *node, node_t *subnode) {
  if (!node->subnodes) {
    // initialize
    node->subnodes = subnode;
    node->subnode_last = subnode;
  } else {
    node->subnode_last->next = subnode;
    node->subnode_last = subnode;
  }

  ++node->subnode_count;
}

node_t *node_clone(node_t *node) {
  node_t *new_node = node_create(node->id);

  // val
  node_set_val(new_node, node->val_buf, node->val_len);
  new_node->val_len = node->val_len;

  // subnodes & next
  node_t *subnode = node->subnodes; // subnode linked list
  node_t *tmp = NULL;
  while (subnode) {
    tmp = subnode->next;
    node_append_subnode(new_node, node_clone(subnode));
    subnode = tmp;
  }

  return new_node;
}

bool node_equal(node_t *node_a, node_t *node_b) {
  if (node_a == node_b) return true;

  if (!node_a || !node_b) return false;

  if (node_a->id != node_b->id) return false;

  if (node_a->val_len != node_b->val_len) return false;

  if (memcmp(node_a->val_buf, node_b->val_buf, node_a->val_len) != 0)
    return false;

  // subnodes
  if (node_a->subnode_count != node_b->subnode_count) return false;

  node_t *subnode_a = node_a->subnodes;
  node_t *tmp_a = NULL;
  node_t *subnode_b = node_b->subnodes;
  node_t *tmp_b = NULL;
  while (subnode_a && subnode_b) {
    tmp_a = subnode_a->next;
    tmp_b = subnode_b->next;

    if (!node_equal(subnode_a, subnode_b))
      return false;

    subnode_a = tmp_a;
    subnode_b = tmp_b;
  }

  if (unlikely(subnode_a != subnode_b))
    return false;

  return true;
}

void _node_to_buf(parsing_tree_t *tree, node_t *node) {
  // dump `val` if this is a leaf node
  if (node->subnode_count == 0) {
    if (node->val_len == 0) return;

    size_t data_len = tree->data_len;
    uint8_t *data_buf = maybe_grow(
      BUF_PARAMS(tree, data), data_len + node->val_len);
    if (!data_buf) {
      perror("tree output buffer allocation (maybe_grow)");
      return;
    }

    memcpy(data_buf + data_len, node->val_buf, node->val_len);
    tree->data_len += node->val_len;

    return;
  }

  // subnodes
  node_t *subnode = node->subnodes; // subnode linked list
  node_t *tmp = NULL;
  while (subnode) {
    tmp = subnode->next;
    _node_to_buf(tree, subnode);
    subnode = tmp;
  }
}



inline parsing_tree_t *tree_create() {
  return calloc(1, sizeof(parsing_tree_t));
}

void tree_free(parsing_tree_t *tree) {
  // root node
  node_free(tree->root);
  tree->root = NULL;

  tree->depth = 0;

  // data buf
  if (tree->data_buf) {
    free(tree->data_buf);
    tree->data_buf = NULL;
    tree->data_size = 0;
    tree->data_len = 0;
  }

  free(tree);
}

void tree_to_buf(parsing_tree_t *tree) {
  maybe_grow(BUF_PARAMS(tree, data), TREE_BUF_PREALLOC_SIZE);
  tree->data_len = 0;

  _node_to_buf(tree, tree->root);
}

parsing_tree_t *tree_from_buf(const uint8_t *data_buf, size_t data_size) {
  return NULL; // TODO: implement this function
}

parsing_tree_t *tree_clone(parsing_tree_t *tree) {
  parsing_tree_t *new_tree = tree_create();
  new_tree->root = node_clone(tree->root);
  new_tree->depth = tree->depth;

  // Do not clone the data buffer, as the cloned tree is likely for mutations
  new_tree->data_buf = NULL;
  new_tree->data_size = 0;
  new_tree->data_len = 0;

  return new_tree;
}

inline bool tree_equal(parsing_tree_t *tree_a, parsing_tree_t *tree_b) {
  if (tree_a == tree_b) return true;
  if (!tree_a || !tree_b) return false;
  return node_equal(tree_a->root, tree_b->root);
}
