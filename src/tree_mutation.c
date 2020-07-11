#include "tree_mutation.h"
#include "json_c_fuzz.h"

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
  max_depth = random() % 15 + 1;  // randomly pick a `max_depth` within [1, 15]
  gen_func_t gen_func = gen_funcs[node->id];
  node_t *   replace_node = gen_func(0);

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

node_t *_pick_recursive_node(node_t *root) {
  // !!!: This function may return NULL
  // TODO: we should uniformly pick recursive nodes in a tree to avoid returning NULL
  if (root->recursion_edge_size != 0 && random() % 2) return root;

  node_t *subnode = NULL;
  node_t *ret = NULL;
  for (int i = 0; i < root->subnode_count; ++i) {
    subnode = root->subnodes[i];
    ret = _pick_recursive_node(subnode);
    if (ret) return ret;
  }

  return NULL;
}

tree_t *random_recursive_mutation(tree_t *tree, uint8_t n) {
  // TODO: currently, we assume `n == 1`. We'd better dump the tree as in the
  //  pre-order traversal way, so we can operate in the array. Then, we need to
  //  convert the array back to a tree

  tree_t *mutated_tree = tree_clone(tree);

  node_t *node = _pick_recursive_node(mutated_tree->root);
  if (node == NULL) {
    // do not change anything
    return mutated_tree;
  }
  int prob = random() % node->recursion_edge_size;

  node_t *subnode = NULL;
  for (int i = 0; i < node->subnode_count; ++i) {
    subnode = node->subnodes[i];
    if (subnode->id == node->id) {
      if (prob == 0) {
        // pick this subnode
        node_t *replace_node = node_clone(node);
        if (node_replace_subnode(node, subnode, replace_node)) {
          node_free(subnode);
        } else {
          node_free(replace_node);
        }
        break;
      }

      prob -= 1;
    }
  }

  return mutated_tree;
}

tree_t *splicing_mutation(tree_t *tree, tree_t *other_tree) {
  // TODO: finish these functions
}
