#ifndef __TREE_H__
#define __TREE_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#include "helpers.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tree_node node_t;
struct tree_node {

  uint32_t id;       // node type
  uint32_t rule_id;  // rule id

  // uint8_t *val_buf;
  // size_t   val_size;
  BUF_VAR(uint8_t, val);
  uint32_t val_len;

  node_t *parent;  // parent node

  node_t **subnodes;
  uint32_t subnode_count;

  // The following two sizes are calculated by `node_get_size`
  size_t recursion_edge_size;  // the total number of recursion edges in the
  // subtree
  size_t non_term_size;  // the number of non-terminal nodes in the subtree

};

typedef struct edge edge_t;
struct edge {

  node_t *parent;
  node_t *subnode;
  size_t  subnode_offset;

};

/**
 * Create a node and allocate the memory
 * @param  id The type of the node
 * @return    A newly created node
 */
node_t *node_create(uint32_t id);

/**
 * Create a node and allocate the memory
 * @param  id      The type of the node
 * @param  rule_id The index of the grammar rule of this node
 * @return         A newly created node
 */
node_t *node_create_with_rule_id(uint32_t id, uint32_t rule_id);

/**
 * Create a node and allocate the memory with a given value
 * @param  id      The type of the node
 * @param  val_buf The buffer of the attached value
 * @param  val_len The size of the attached value
 * @return         A newly created node
 */
node_t *node_create_with_val(uint32_t id, const void *val_buf, size_t val_len);

/**
 * Initialize the subnode array.
 * @param node The node
 * @param n    The size of the subnode array
 */
void node_init_subnodes(node_t *node, size_t n);

/**
 * Destroy the node and recursively free all memory
 * @param node The node
 */
void node_free(node_t *node);

/**
 * Destroy the node and free its memory but do not recurse
 * and destroy the subnodes.
 * This is dangerous and should only be used if you plan to
 * manually free all the subnodes as well!
 * @param node The node
 */
void node_free_only_self(node_t *node);

/**
 * Set the concrete value for the node
 * @param node    The node
 * @param val_buf The buffer of the attached value
 * @param val_len The size of the attached value
 */
void node_set_val(node_t *node, const void *val_buf, size_t val_len);

/**
 * Set i-th subnode. `i` should be less than the number of subnodes. (i.e., i <
 * node->subnode_count)
 * @param node    The node
 * @param i       The index of the subnode
 * @param subnode The added subnode
 */
void node_set_subnode(node_t *node, size_t i, node_t *subnode);

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
 * Calculate the total number of non-terminal subnodes and the total number of
 * recursive edges in the tree
 * @param  node The root node of a tree
 */
void node_get_size(node_t *node);

/**
 * Replace `subnode` in a stree (`root`) with `new_subnode`. Note that, this
 * function will not destroy `subnode` and free its memory.
 * @param root         The root node of a tree
 * @param subnode      A subnode or the root node
 * @param replace_node A new subnode that will be added
 * @return             True (1) if the subnode has been successfully replaced;
 *                     otherwise, false (0)
 */
bool node_replace_subnode(node_t *root, node_t *subnode, node_t *new_subnode);

/**
 * Uniformly pick a subnode in a tree (`node`). As we track the number of
 * non-terminal nodes while adding the subnode (see `node_append_subnode`), for
 * each node node in a tree, the probability of being selected is `1 / the
 * number of non-terminal subnodes in the tree`.
 *
 * Alternative solution: We can first apply pre-order traversal on the tree
 * and dump the tree to an array. Then, randomly pick an element in the array.
 * @param node The root node of a tree
 * @return     The randomly picked subnode
 */
node_t *node_pick_non_term_subnode(node_t *node);

/**
 * Similar to `node_pick_non_term_subnode`, this function uniformly picks a
 * recursion edge, in which two end points have the same node type (i.e., `id`).
 * @param node The root node of a tree
 * @return     The randomly picked recursion edge
 */
edge_t node_pick_recursion_edge(node_t *node);

/**
 * Find the edge between the given `node` and its parent `node->parent`, if any
 * @param node The root node of a tree
 * @return     The edge between the given node and its parent; otherwise, NULL
 */
edge_t node_get_parent_edge(node_t *node);

typedef struct tree {

  node_t *root;

  // uint8_t *data_buf;
  // size_t   data_size;
  BUF_VAR(uint8_t, data);
  size_t data_len;  // data_len <= data_size

  // uint8_t *ser_buf;
  // size_t   ser_size;
  BUF_VAR(uint8_t, ser);
  size_t ser_len;  // ser_len <= ser_size

  list_t *non_terminal_node_list;
  list_t *recursion_edge_list;

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
 * @param  tree The parsing tree
 */
void tree_to_buf(tree_t *tree);

/**
 * Parse the given buffer to construct a parsing tree
 * @param  data_buf  The buffer of a test case
 * @param  data_size The size of the buffer
 * @return           A newly created tree
 */
tree_t *tree_from_buf(const uint8_t *data_buf, size_t data_size);

/**
 * Serialize a given tree into binary data
 * @param tree    A given tree
 */
void tree_serialize(tree_t *tree);

/**
 * Deserialize the data to recover a tree
 * @param data_buf  The buffer of a serialized tree
 * @param data_size The size of the buffer
 * @return          A newly created tree
 */
tree_t *tree_deserialize(const uint8_t *data_buf, size_t data_size);

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

/**
 * Calculate the size of a given tree (i.e., the total number of non-terminal
 * nodes in a tree).
 * @param  tree A given tree
 * @return      The total number of non-terminal nodes in this tree
 */
size_t tree_get_size(tree_t *tree);

/**
 * Get all recursion edges in the tree, and store them in a linked list
 * @param tree A given tree
 */
void tree_get_recursion_edges(tree_t *tree);

/**
 * Get all non-terminal nodes in the tree, and store them in a linked list
 * @param tree A given tree
 */
void tree_get_non_terminal_nodes(tree_t *tree);

/**
 * Read/Deserialize a tree from a file
 * @param filename The path to the tree file
 * @return         The deserialized tree
 */
tree_t *read_tree_from_file(const char *filename);

/**
 * Load/Parse a tree from a test case file
 * @param filename The path to the fuzzing test case
 * @return         The parsed tree
 */
tree_t *load_tree_from_test_case(const char *filename);

/**
 * Write/Serialize a tree to a file
 * @param tree     The tree to be written to the file
 * @param filename The path to the tree file
 */
void write_tree_to_file(tree_t *tree, const char *filename);

/**
 * Dump a tree to a test case file
 * @param tree     The tree to be written to the file
 * @param filename The path to the test case file
 */
void dump_tree_to_test_case(tree_t *tree, const char *filename);

#ifdef __cplusplus
}
#endif

#endif
