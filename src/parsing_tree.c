#include "parsing_tree.h"

#define TREE_BUF_PREALLOC_SIZE (64)

void inline node_append_subnode(node_t *node, node_t *subnode) {
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

  // LIST_FOREACH(&node->subnodes, node_t, {
  //   _node_to_buf(tree, el);
  // });

  do {

    list_t *   li = (&node->subnodes);
    element_t *head = get_head((li));
    element_t *el_box = (head)->next;
    if (!el_box)
      FATAL("foreach over uninitialized list");
    while (el_box != head) {

      node_t *el = (node_t *)((el_box)->data);
      /* get next so el_box can be unlinked */
      element_t *next = el_box->next;
      _node_to_buf(tree, el);
      el_box = next;

    }

  } while (0);
}

void tree_to_buf(parsing_tree_t *tree) {
  maybe_grow(BUF_PARAMS(tree, data), TREE_BUF_PREALLOC_SIZE);
  tree->data_len = 0;

  _node_to_buf(tree, tree->root);
}
