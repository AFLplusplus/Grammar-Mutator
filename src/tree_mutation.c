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

#include "tree_mutation.h"
#include "f1_c_fuzz.h"
#include "chunk_store.h"

static size_t max_tree_len = 1000;

void tree_set_max_len(size_t max_len) {

  max_tree_len = max_len;

}

tree_t *random_mutation(tree_t *tree) {

  if (unlikely(!tree)) return NULL;

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
  gen_func_t gen_func = gen_funcs[node->id];
  int        consumed = 0;
  node_t *   replace_node = gen_func(max_tree_len, &consumed, -1);

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

tree_t *rules_mutation(tree_t *tree, node_t *node, uint32_t rule_id) {

  if (unlikely(!tree)) return NULL;
  if (unlikely(!node)) return NULL;
  if (unlikely(node->id == 0)) return NULL;
  if (unlikely(rule_id >= node_num_rules[node->id])) return NULL;

  // Note: this may not be an error case
  if (unlikely(node->rule_id == rule_id)) return NULL;

  tree_t *mutated_tree = NULL;
  node_t *parent = node->parent;

  // Generate a new node
  gen_func_t gen_func = gen_funcs[node->id];
  int        consumed = 0;
  node_t *   replace_node = gen_func(max_tree_len, &consumed, rule_id);

  if (!parent) {

    // no parent, meaning that the input node is the root node in the tree
    mutated_tree = tree_create();
    mutated_tree->root = replace_node;
    return mutated_tree;

  }

  // detach `node` from `parent`
  edge_t edge = node_get_parent_edge(node);
  node->parent = NULL;

  // attach `replace_node` to the original position of `node` in `parent`
  parent->subnodes[edge.subnode_offset] = replace_node;
  replace_node->parent = parent;

  mutated_tree = tree_clone(tree);

  // recover `tree`
  parent->subnodes[edge.subnode_offset] = node;
  node->parent = parent;

  node_free(replace_node);

  return mutated_tree;

}

size_t _node_rules_mutation_count(node_t *node) {

  if (unlikely(!node)) return 0;
  if (node->id == 0) return 0;
  if (unlikely(node_num_rules[node->id] <= 0)) return 0;

  size_t ret = node_num_rules[node->id] - 1;

  node_t *subnode;
  for (size_t i = 0; i < node->subnode_count; ++i) {

    subnode = node->subnodes[i];
    ret += _node_rules_mutation_count(subnode);

  }

  return ret;

}

size_t rules_mutation_count(tree_t *tree) {

  if (unlikely(!tree)) return 0;
  return _node_rules_mutation_count(tree->root);

}

tree_t *random_recursive_mutation(tree_t *tree, uint8_t n) {

  if (unlikely(!tree)) return NULL;

  tree_t *mutated_tree = tree_clone(tree);

  edge_t picked_edge = node_pick_recursion_edge(mutated_tree->root);
  if (picked_edge.parent == NULL || picked_edge.subnode == NULL) {

    // no recursion edge, return the original one
    return mutated_tree;

  }

  node_t *parent = picked_edge.parent;
  node_t *tail = picked_edge.subnode;
  size_t  offset = picked_edge.subnode_offset;

  // detach the tail
  tail->parent = NULL;
  parent->subnodes[offset] = NULL;

  size_t  num = 1 << n;
  node_t *cloned_part = NULL;
  for (size_t i = 0; i < num; ++i) {

    cloned_part = node_clone(parent);

    // attach the tail to the cloned part
    tail->parent = cloned_part;
    cloned_part->subnodes[offset] = tail;

    tail = cloned_part;

  }

  // attach the tail to the parent
  tail->parent = parent;
  parent->subnodes[offset] = tail;

  return mutated_tree;

}

tree_t *splicing_mutation(tree_t *tree) {

  if (unlikely(!tree)) return NULL;

  tree_t *mutated_tree = tree_clone(tree);

  // randomly pick a node in the tree
  node_t *node = node_pick_non_term_subnode(mutated_tree->root);
  if (unlikely(node == NULL)) {

    // By design, _pick_non_term_node should not return NULL
    perror("_pick_non_term_node returns NULL");
    exit(EXIT_FAILURE);

  }

  node_t *parent = node->parent;

  // pick a subtree, in which the root type is the same as the picked node, from
  // the chunk store
  node_t *replace_node = chunk_store_get_alternative_node(node);
  if (!replace_node) {

    // if there is no alternative node, return the cloned tree
    return mutated_tree;

  }

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
