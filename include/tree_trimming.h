#ifndef __TREE_TRIMMING_H__
#define __TREE_TRIMMING_H__

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
 * For each nonterminal, this function generates the smallest possible subtree.
 * Then, this function sequentially replaces each node’s subtree with the
 * smallest possible subtree at this position
 * @param  tree A tree
 * @param  node The non-terminal node that needs to be trimmed
 * @return      A tree with smaller size
 */
tree_t *subtree_trimming(tree_t *tree, node_t *node);

/**
 * Reduce the amount of recursions by identifying recursions and replacing them
 * one at a time
 * @param  tree A tree
 * @param  edge The recursion edge that needs to be trimmed
 * @return      A tree with smaller size
 */
tree_t *recursive_trimming(tree_t *tree, edge_t edge);

#ifdef __cplusplus
}
#endif

#endif
