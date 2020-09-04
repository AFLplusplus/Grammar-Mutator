/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Written by Shengtuo Hu

   Copyright 2020 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

#include "f1_c_fuzz.h"
#include "tree_trimming.h"

tree_t *subtree_trimming(tree_t *tree, node_t *node) {

  tree_t *trimmed_tree = NULL;

  // generate the minimal subtree
  gen_func_t gen_func = gen_funcs[node->id];
  int        consumed = 0;
  node_t *   min_node = gen_func(0, &consumed, -1);

  node_t *parent = node->parent;
  if (!parent) {

    // no parent, meaning that the input node is the root node in the tree
    trimmed_tree = tree_create();
    trimmed_tree->root = min_node;
    return trimmed_tree;

  }

  // detach `node` from `parent`
  edge_t edge = node_get_parent_edge(node);
  node->parent = NULL;

  // attach `min_node` to the original position of `node` in `parent`
  parent->subnodes[edge.subnode_offset] = min_node;
  min_node->parent = parent;

  trimmed_tree = tree_clone(tree);

  // recover `tree`
  parent->subnodes[edge.subnode_offset] = node;
  node->parent = parent;

  node_free(min_node);

  return trimmed_tree;

}

tree_t *recursive_trimming(tree_t *tree, edge_t edge) {

  tree_t *trimmed_tree = NULL;

  node_t *parent = edge.parent;
  node_t *tail = edge.subnode;
  if (!parent || !tail) {

    // if `edge` is empty, return the original tree
    trimmed_tree = tree_clone(tree);
    return trimmed_tree;

  }

  node_t *pre_parent = parent->parent;
  if (!pre_parent) {

    // if `parent` is the root node of the tree, return the tail part
    trimmed_tree = tree_create();
    trimmed_tree->root = node_clone(tail);
    return trimmed_tree;

  }

  // detach `tail` from `parent`
  tail->parent = NULL;

  // get the edge between `parent` and `pre_parent`
  edge_t pre_edge = node_get_parent_edge(parent);

  // detach `parent` from `pre_parent`
  size_t pre_offset = pre_edge.subnode_offset;
  parent->parent = NULL;

  // attach `tail` to the original position of `parent` in `pre_parent`
  pre_parent->subnodes[pre_offset] = tail;
  tail->parent = pre_parent;

  trimmed_tree = tree_clone(tree);

  // recover `tree`
  pre_parent->subnodes[pre_offset] = parent;
  parent->parent = pre_parent;
  tail->parent = parent;

  return trimmed_tree;

}
