#ifndef __TREE_H__
#define __TREE_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

// AFL++
#include <list.h>
#include <alloc-inl.h>

#include "helpers.h"

typedef struct tree_node {
  // uint8_t *val_buf;
  // size_t   val_size;
  BUF_VAR(uint8_t, val);

  // TODO: the initial size of `list_t` may be too large
  list_t subnodes;
  size_t subnode_count;
} node_t;

/**
 * Create a node and allocate the memory
 * @return A newly created node
 */
node_t *node_create();

/**
 * Create a node and allocate the memory with a given value
 * @param  val_buf  The buffer of the attached value
 * @param  val_size The size of the attached value
 * @return          A newly created node
 */
node_t *node_create_with_val(const uint8_t *val_buf, size_t val_size);

/**
 * Destroy the node and recursively free all memory
 * @param node The node
 */
void node_free(node_t *node);

/**
 * Set the concrete value for the node
 * @param node     The node
 * @param val_buf  The buffer of the attached value
 * @param val_size The size of the attached value
 */
void node_set_val(node_t *node, const uint8_t *val_buf, size_t val_size);

/**
 * Append a child node `subnode` to a parent node `node`
 * @param node    The parent node
 * @param subnode The child node
 */
void node_append_subnode(node_t *node, node_t *subnode);



typedef struct parsing_tree {
  node_t *root;
  size_t depth;

  // uint8_t *data_buf;
  // size_t   data_size;
  BUF_VAR(uint8_t, data);
  size_t data_len;  // data_len <= data_size
} parsing_tree_t;

/**
 * Create a tree and allocate the memory
 * @return A newly created tree
 */
parsing_tree_t *tree_create();

/**
 * Destroy the tree and free all memory
 * @param tree The parsing tree
 */
void tree_free(parsing_tree_t *tree);

/**
 * Convert a parsing tree into a concrete test case stored in the data buffer
 * @param  tree    The parsing tree
 */
void tree_to_buf(parsing_tree_t *tree);

/**
 * Parse the given buffer to construct a parsing tree
 * @param  data_buf  The buffer of serialized data
 * @param  data_size The size of the buffer
 * @return           A newly created tree
 */
parsing_tree_t *tree_from_buf(const uint8_t *data_buf, size_t data_size);

#endif
