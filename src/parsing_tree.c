#include "parsing_tree.h"

#define TREE_BUF_PREALLOC_SIZE (64)


inline node_t *node_create() {
  return calloc(1, sizeof(node_t));
}

node_t *node_create_with_val(const uint8_t *val_buf, size_t val_size) {
  node_t *node = node_create();

  node_set_val(node, val_buf, val_size);

  return node;
}

void node_free(node_t *node) {
  // val buf
  if (node->val_buf) {
    free(node->val_buf);
    node->val_buf = NULL;
    node->val_size = 0;
  }

  // subnodes
  if (node->subnode_count != 0) {
    LIST_FOREACH(&node->subnodes, node_t, {
      node_free(el);
    });
  }

  free(node);
}

void node_set_val(node_t *node, const uint8_t *val_buf, size_t val_size) {
  node->val_buf = malloc(val_size * sizeof(uint8_t));
  node->val_size = val_size;
  memcpy(node->val_buf, val_buf, val_size);
}

inline void node_append_subnode(node_t *node, node_t *subnode) {
  list_append(&node->subnodes, subnode);
  ++node->subnode_count;
}

void _node_to_buf(parsing_tree_t *tree, node_t *node) {
  // dump `val` if this is a leaf node
  if (node->subnode_count == 0) {
    if (node->val_size == 0) return;

    size_t data_len = tree->data_len;
    uint8_t *data_buf = maybe_grow(
      BUF_PARAMS(tree, data), data_len + node->val_size);
    if (!data_buf) {
      perror("tree output buffer allocation (maybe_grow)");
      return;
    }

    memcpy(data_buf + data_len, node->val_buf, node->val_size);
    tree->data_len += node->val_size;

    return;
  }

  LIST_FOREACH(&node->subnodes, node_t, {
    _node_to_buf(tree, el);
  });
}



inline parsing_tree_t *tree_create() {
  return calloc(1, sizeof(parsing_tree_t));
}

void tree_free(parsing_tree_t *tree) {
  // root node
  node_free(tree->root);
  tree->root = NULL;

  // data buf
  if (tree->data_buf) {
    free(tree->data_buf);
    tree->data_buf = NULL;
    tree->data_size = 0;
  }

  free(tree);
}

void tree_to_buf(parsing_tree_t *tree) {
  maybe_grow(BUF_PARAMS(tree, data), TREE_BUF_PREALLOC_SIZE);
  tree->data_len = 0;

  _node_to_buf(tree, tree->root);
}
