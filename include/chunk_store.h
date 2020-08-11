#ifndef __CHUNK_STORE_H__
#define __CHUNK_STORE_H__

#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the chunk store
 */
void chunk_store_init();

/**
 * Add all subtrees in a tree to the chunk store
 * @param tree A given tree
 */
void chunk_store_add_tree(tree_t *tree);

/**
 * Get a seen node from the chunk store, which has the same type as the given
 * `node`
 * @param node The given node
 * @return     An alternative node that has the same type as `node`
 */
node_t *chunk_store_get_alternative_node(node_t *node);

/**
 * Clear all stored chunks
 */
void chunk_store_clear();

#ifdef __cplusplus
}
#endif

#endif
