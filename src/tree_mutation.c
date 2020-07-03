#include "tree_mutation.h"

node_t *_pick_node(node_t *root, size_t tree_size) {
  return NULL;
}

tree_t *random_mutation(tree_t *tree) {
  tree_t *mutated_tree = tree_clone(tree);

  // Randomly pick a node in the tree
  node_t *node = _pick_node(mutated_treea);
  node_t *parent = node->parent;

  // Generate a new node
  max_depth = random() % 15 + 1; // randomly pick a `max_depth` within [1, 15]
  gen_func_t gen_func = gen_funcs[node->id];
  node_t *replace_node = gen_func(0);

  if (!parent) { // no parent, meaning that the picked node is the root node
    // Destroy the original root node
    node_free(node);
    // Simply set the new root node
    mutated_tree->root = replace_node;
    return mutated_tree;
  }

  node_replace_subnode(parent, node, replace_node);
  node_free(node);

  return mutated_tree;
}

tree_t *rules_mutation(tree_t *tree) {
    // TODO: finish these functions
}

tree_t *random_recursive_mutation(tree_t *tree, uint8_t n) {
    // TODO: finish these functions
}

tree_t *splicing_mutation(tree_t *tree, tree_t *other_tree) {
    // TODO: finish these functions
}
