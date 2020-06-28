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

#include "common.h"

typedef struct tree_node {
  // uint8_t *val_buf;
  // size_t   val_size;
  BUF_VAR(uint8_t, val);

  list_t subnodes;
  size_t subnode_count;
} node_t;

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
 * Convert a parsing tree into a concrete test case stored in the data buffer
 * @param  tree    The parsing tree
 */
void tree_to_buf(parsing_tree_t *tree);

#endif
