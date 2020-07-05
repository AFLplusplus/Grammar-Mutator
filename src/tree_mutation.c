#include "tree_mutation.h"
#include "json_c_fuzz.h"

node_t *_pick_non_term_node(node_t *root) {
  /**
   * As we track the number of non-terminal nodes while adding the subnode (see
   * `node_append_subnode` in `tree.h`/`tree.c`), for each root node in a tree,
   * the probability of being selected is `1 / the number of non-terminal
   * subnodes in the tree`.
   *
   * Alternative solution: We can first apply pre-order traversal on the tree
   * and dump the tree to an array. Then, randomly pick an element in the array.
   */

  size_t non_term_size = root->non_term_size;
  int    prob = random() % non_term_size;
  if (prob < 1) return root;
  prob -= 1;

  node_t *subnode = root->subnodes;
  node_t *tmp = NULL;
  while (subnode) {
    tmp = subnode->next;

    if (subnode->id != TERM_NODE) {
      if (prob < subnode->non_term_size) return _pick_non_term_node(subnode);

      prob -= subnode->non_term_size;
    }

    subnode = tmp;
  }

  // should not reach here
  return NULL;
}

tree_t *random_mutation(tree_t *tree) {
  tree_t *mutated_tree = tree_clone(tree);

  // Randomly pick a node in the tree
  node_t *node = _pick_non_term_node(mutated_tree->root);
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
