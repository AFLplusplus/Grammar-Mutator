#include "parsing_tree.h"

#define TREE_BUF_PREALLOC_SIZE (64)

void _to_buf(parsing_tree_t *tree, node_t *node) {
  // dump `val` if this is the leaf node
  if (node->children.element_prealloc_count == 0) {
    if (node->val_size == 0) return;

    size_t data_len = tree->data_len;
    uint8_t *data_buf = maybe_grow(
      BUF_PARAMS(tree, data), data_len + node->val_size);
    if (!mutated_out) {
      perror("tree output buffer allocation (maybe_grow)");
      return;
    }

    memcpy(data_buf + data_len, node->val_buf, node->val_size);
    tree->data_len += node->val_size;

    return;
  }

  LIST_FOREACH(&node->children, node_t, {
    _to_buf(tree, el);
  });
}

void to_buf(parsing_tree_t *tree) {
  maybe_grow(BUF_PARAMS(tree, data), TREE_BUF_PREALLOC_SIZE);
  tree->data_len = 0;

  _to_buf(tree, tree->root);
}
