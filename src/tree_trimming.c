#include "tree_trimming.h"

tree_t *subtree_trimming(tree_t *tree) {
  return NULL;  // TODO: implement this
}

tree_t *recursive_trimming(tree_t *tree, edge_t edge) {
  tree_t *trimmed_tree = NULL;

  node_t *parent = edge.parent;
  node_t *tail = edge.subnode;

  node_t *pre_parent = parent->parent;
  if (!pre_parent) {
    // `parent` is the root node of the tree
    // return the tail part
    trimmed_tree = tree_create();
    trimmed_tree->root = node_clone(tail);
    return trimmed_tree;
  }

  // detach `tail` from `parent`
  tail->parent = NULL;

  // get the edge between `parent` and `pre_parent`
  edge_t pre_edge = node_get_parent_edge(parent);

  // detach `parent` from `pre_parent`
  size_t pre_offset = pre_edge.subnode_offset;
  parent->parent = NULL;

  // attach `tail` to the original position of `parent` in `pre_parent`
  pre_parent->subnodes[pre_offset] = tail;
  tail->parent = pre_parent;

  trimmed_tree = tree_clone(tree);

  // recover `tree`
  pre_parent->subnodes[pre_offset] = parent;
  parent->parent = pre_parent;
  tail->parent = parent;

  return trimmed_tree;
}