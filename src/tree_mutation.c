#include "tree_mutation.h"
#include "f1_c_fuzz.h"
#include "chunk_store.h"

tree_t *random_mutation(tree_t *tree) {
  tree_t *mutated_tree = tree_clone(tree);

  // Randomly pick a node in the tree
  node_t *node = node_pick_non_term_subnode(mutated_tree->root);
  if (unlikely(node == NULL)) {
    // By design, _pick_non_term_node should not return NULL
    perror("_pick_non_term_node returns NULL");
    exit(EXIT_FAILURE);
  }
  node_t *parent = node->parent;

  // Generate a new node
  gen_func_t gen_func = gen_funcs[node->id];
  int        consumed = 0;
  node_t *   replace_node = gen_func(MAX_TREE_LEN, &consumed, -1);

  if (!parent) {  // no parent, meaning that the picked node is the root node
    // Destroy the original root node
    node_free(node);
    // Simply set the new root node
    mutated_tree->root = replace_node;
    return mutated_tree;
  }

  if (node_replace_subnode(parent, node, replace_node)) {
    node_free(node);
  } else {
    node_free(replace_node);
  }

  return mutated_tree;
}

tree_t *rules_mutation(tree_t *tree) {
  // TODO: finish these functions
}

tree_t *random_recursive_mutation(tree_t *tree, uint8_t n) {
  tree_t *mutated_tree = tree_clone(tree);

  edge_t picked_edge = node_pick_recursion_edge(mutated_tree->root);
  if (picked_edge.parent == NULL || picked_edge.subnode == NULL) {
    // no recursion edge, return the original one
    return mutated_tree;
  }

  node_t *parent = picked_edge.parent;
  node_t *tail = picked_edge.subnode;
  size_t  offset = picked_edge.subnode_offset;

  // detach the tail
  tail->parent = NULL;
  parent->subnodes[offset] = NULL;

  int     num = 1 << n;
  node_t *cloned_part = NULL;
  for (int i = 0; i < num; ++i) {
    cloned_part = node_clone(parent);

    // attach the tail to the cloned part
    tail->parent = cloned_part;
    cloned_part->subnodes[offset] = tail;

    tail = cloned_part;
  }

  // attach the tail to the parent
  tail->parent = parent;
  parent->subnodes[offset] = tail;

  return mutated_tree;
}

tree_t *splicing_mutation(tree_t *tree) {
  tree_t *mutated_tree = tree_clone(tree);

  // randomly pick a node in the tree
  node_t *node = node_pick_non_term_subnode(mutated_tree->root);
  if (unlikely(node == NULL)) {
    // By design, _pick_non_term_node should not return NULL
    perror("_pick_non_term_node returns NULL");
    exit(EXIT_FAILURE);
  }
  node_t *parent = node->parent;

  // pick a subtree, in which the root type is the same as the picked node, from
  // the chunk store
  node_t *replace_node = chunk_store_get_alternative_node(node);
  if (!replace_node) {
    // if there is no alternative node, return the cloned tree
    return mutated_tree;
  }

  if (!parent) {  // no parent, meaning that the picked node is the root node
    // Destroy the original root node
    node_free(node);
    // Simply set the new root node
    mutated_tree->root = replace_node;
    return mutated_tree;
  }

  if (node_replace_subnode(parent, node, replace_node)) {
    node_free(node);
  } else {
    node_free(replace_node);
  }

  return mutated_tree;
}
