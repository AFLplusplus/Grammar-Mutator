#ifndef __TREE_MUTATION_H__
#define __TREE_MUTATION_H__

#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Reference: "NAUTILUS: Fishing for Deep Bugs with Grammars", NDSS 2019
 * Link:
 * https://www.syssec.ruhr-uni-bochum.de/media/emma/veroeffentlichungen/2018/12/17/NDSS19-Nautilus.pdf
 */
/**
 * Pick a random node of a tree and replace it with a randomly-generated new
 * subtree rooted in the same type.
 * @param  tree A parsing tree
 * @return      A mutated parsing tree
 */
tree_t *random_mutation(tree_t *tree);

/**
 * Sequentially replace each node of the input tree with one subtree generated
 * by all other possible rules.
 * @param  tree A parsing tree
 * @return      A mutated parsing tree
 */
tree_t *rules_mutation(tree_t *tree);

/**
 * Pick a random recursion of a tree and repeats that recursion 2^n times
 * (0 < n < 16). This creates trees with higher degree of nesting.
 * @param  tree A parsing tree
 * @param  n    Recursion factor
 * @return      A mutated parsing tree
 */
tree_t *random_recursive_mutation(tree_t *tree, uint8_t n);

/**
 * Combine inputs that found different paths by taking a subtree from one
 * interesting input and placing it in another input: it replaces one of the
 * subtrees with a “fitting” subtree from another tree in the queue. To do so,
 * it picks a random internal node, which becomes the root of the subtree to be
 * replaced. Then it picks from a tree in the queue a random subtree that is
 * rooted in the same nonterminal to replace the old subtree.
 * @param  tree A parsing tree
 * @return      A mutated parsing tree
 */
tree_t *splicing_mutation(tree_t *tree);

#ifdef __cplusplus
}
#endif

#endif
