#ifndef __TREE_H__
#define __TREE_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#include "helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tree_node node_t;
struct tree_node {
  uint32_t id; // node type

  // uint8_t *val_buf;
  // size_t   val_size;
  BUF_VAR(uint8_t, val);
  size_t val_len;

  node_t *parent; // parent node

  node_t *subnodes;
  node_t *subnode_last; // last subnode
  size_t subnode_count;

  node_t *next;
};

/**
 * Create a node and allocate the memory
 * @param  id The type of the node
 * @return    A newly created node
 */
node_t *node_create(uint32_t id);

/**
 * Create a node and allocate the memory with a given value
 * @param  id      The type of the node
 * @param  val_buf The buffer of the attached value
 * @param  val_len The size of the attached value
 * @return         A newly created node
 */
node_t *node_create_with_val(uint32_t id, const void *val_buf, size_t val_len);

/**
 * Destroy the node and recursively free all memory
 * @param node The node
 */
void node_free(node_t *node);

/**
 * Set the concrete value for the node
 * @param node    The node
 * @param val_buf The buffer of the attached value
 * @param val_len The size of the attached value
 */
void node_set_val(node_t *node, const void *val_buf, size_t val_len);

/**
 * Append a child node `subnode` to a parent node `node`
 * @param node    The parent node
 * @param subnode The child node
 */
void node_append_subnode(node_t *node, node_t *subnode);

/**
 * Clone a node, including all subnodes
 * @param  node The node
 * @return      A newly created node with the same data as `node`
 */
node_t *node_clone(node_t *node);

/**
 * Compare recursively whether two nodes have the same values and subnodes
 * @param  node_a One node
 * @param  node_b Another node
 * @return        True (1) if two nodes are the same; otherwise, false (0)
 */
bool node_equal(node_t *node_a, node_t *node_b);

/**
 * Calculate the size of a given node/subtree.
 * @param  node The node/subtree
 * @return      The total number of nodes in this subtree
 */
size_t node_get_size(node_t *node);

/**
 * Replace `subnode` in a subtree (`root`) with `new_subnode`. Note that, this
 * function will not destroy `subnode` and free its memory.
 * @param root         The root node of a tree/subtree
 * @param subnode      A subnode or the root node
 * @param replace_node A new subnode that will be added
 */
/**
 * Replace `subnode` in a subtree (`root`) with `new_subnode`. Note that, this
 * function will not destroy `subnode` and free its memory.
 * @param root         The root node of a tree/subtree
 * @param subnode      A subnode or the root node
 * @param replace_node A new subnode that will be added
 * @return             True (1) if the subnode has been successfully replaced;
 *                     otherwise, false (0)
 */
bool node_replace_subnode(node_t *root, node_t *subnode, node_t *new_subnode);

typedef struct tree {
  node_t *root;
  size_t  depth;

  // uint8_t *data_buf;
  // size_t   data_size;
  BUF_VAR(uint8_t, data);
  size_t data_len;  // data_len <= data_size
} tree_t;

/**
 * Create a tree and allocate the memory
 * @return A newly created tree
 */
tree_t *tree_create();

/**
 * Destroy the tree and free all memory
 * @param tree The parsing tree
 */
void tree_free(tree_t *tree);

/**
 * Convert a parsing tree into a concrete test case stored in the data buffer
 * @param  tree    The parsing tree
 */
void tree_to_buf(tree_t *tree);

/**
 * Parse the given buffer to construct a parsing tree
 * @param  data_buf  The buffer of serialized data
 * @param  data_size The size of the buffer
 * @return           A newly created tree
 */
tree_t *tree_from_buf(const uint8_t *data_buf, size_t data_size);

/**
 * Clone a parsing tree
 * @param  tree The parsing tree
 * @return      A newly created tree with the same data as `tree`
 */
tree_t *tree_clone(tree_t *tree);

/**
 * Compare whether two parsing trees have the same architecture, and
 * corresponding nodes have the same values
 * @param  tree_a One paring tree
 * @param  tree_b Another parsing tree
 * @return        True (1) if two trees are the same; otherwise, false (0)
 */
bool tree_equal(tree_t *tree_a, tree_t *tree_b);

#ifdef __cplusplus
}
#endif

#endif
