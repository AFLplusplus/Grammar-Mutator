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
#include "node_stack.h"

static size_t max_tree_len = 1000;

void tree_set_max_len(size_t max_len) {

  max_tree_len = max_len;

}

tree_t *random_mutation(tree_t *tree) {

  if (unlikely(!tree)) return NULL;

  tree_t *mutated_tree = tree_clone(tree);
  if (unlikely(!mutated_tree)) return NULL;

  // Randomly pick a node in the tree
  node_t *node = node_pick_non_term_subnode(mutated_tree->root);
  if (unlikely(node == NULL)) {

    // The sizes of the tree are 0, for example after a failed size walk
    fprintf(stderr, "The tree has no node to pick (%s)\n", __func__);
    tree_free(mutated_tree);
    return NULL;

  }

  node_t *parent = node->parent;

  // Generate a new node
  gen_func_t gen_func = gen_funcs[node->id];
  int        consumed = 0;
  node_t    *replace_node = gen_func(max_tree_len, &consumed, -1);

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
  node_t    *replace_node = gen_func(max_tree_len, &consumed, rule_id);

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

  // If a push fails, the function returns 0: no rules mutation runs.
  size_t       ret = 0;
  node_stack_t stack = {0};
  bool         ok = !node || node_stack_push(&stack, node, 0, 0);

  while (ok && stack.len) {

    node = node_stack_pop(&stack).node;
    if (node->id == 0) continue;
    if (unlikely(node_num_rules[node->id] <= 0)) continue;

    // An unknown rule id has no skipped alternative, so count every rule.
    ret += node->rule_id >= node_num_rules[node->id]
               ? node_num_rules[node->id]
               : node_num_rules[node->id] - 1;

    ok = node_stack_push_children(&stack, node, 0);

  }

  node_stack_free(&stack);
  return ok ? ret : 0;

}

size_t rules_mutation_count(tree_t *tree) {

  if (unlikely(!tree)) return 0;
  return _node_rules_mutation_count(tree->root);

}

// The number of bytes that a subtree serializes to
static size_t node_data_len(node_t *node) {

  // If a push fails, the function returns SIZE_MAX. The growth check then
  // rejects the mutation.
  size_t       data_len = 0;
  node_stack_t stack = {0};
  bool         ok = !node || node_stack_push(&stack, node, 0, 0);

  while (ok && stack.len) {

    node = node_stack_pop(&stack).node;
    if (node->subnode_count == 0) {

      data_len += node->val_len;
      continue;

    }

    ok = node_stack_push_children(&stack, node, 0);

  }

  node_stack_free(&stack);
  return ok ? data_len : SIZE_MAX;

}

// Return the node at the place of "node" in a copy of its tree. The function
// finds the position of "node" and of each parent in the parent's subnodes, and
// follows the same positions down from "clone_root". Return NULL if the copy
// has no node at that place, or if memory runs out.
static node_t *node_at_same_place(const node_t *node, node_t *clone_root) {

  size_t depth = 0;
  for (const node_t *cur = node; cur->parent; cur = cur->parent)
    ++depth;
  if (!depth) return clone_root;

  uint32_t *positions = malloc(depth * sizeof(uint32_t));
  if (unlikely(!positions)) return NULL;

  size_t i = depth;
  for (const node_t *cur = node; cur->parent; cur = cur->parent) {

    const node_t *parent = cur->parent;
    uint32_t      k = 0;
    while (k < parent->subnode_count && parent->subnodes[k] != cur)
      ++k;
    positions[--i] = k;

  }

  node_t *cur = clone_root;
  for (i = 0; i < depth && cur; ++i)
    cur =
        positions[i] < cur->subnode_count ? cur->subnodes[positions[i]] : NULL;

  free(positions);
  return cur;

}

tree_t *random_recursive_mutation(tree_t *tree, uint8_t n, size_t max_growth) {

  if (unlikely(!tree)) return NULL;

  // Pick the edge and check the growth on the original tree. A failed check
  // then costs no copy of the tree.
  edge_t picked_edge = node_pick_recursion_edge(tree->root);
  if (picked_edge.parent == NULL || picked_edge.subnode == NULL) {

    // no recursion edge, return a copy of the original one
    return tree_clone(tree);

  }

  size_t offset = picked_edge.subnode_offset;
  size_t num = 1 << n;

  // Every repetition adds the parent without the tail to the input. Check it
  // before the repetitions are built, which may need much memory.
  size_t parent_len = node_data_len(picked_edge.parent);
  size_t tail_len = node_data_len(picked_edge.subnode);
  if (parent_len == SIZE_MAX || tail_len == SIZE_MAX ||
      (parent_len - tail_len) * num > max_growth) {

    return NULL;

  }

  tree_t *mutated_tree = tree_clone(tree);
  if (unlikely(!mutated_tree)) return NULL;

  // Use the same edge in the copy. If memory ran out during the copy, a node of
  // the copy can have no subnodes, so check the tail too.
  picked_edge.parent =
      node_at_same_place(picked_edge.parent, mutated_tree->root);
  if (unlikely(!picked_edge.parent ||
               offset >= picked_edge.parent->subnode_count ||
               !picked_edge.parent->subnodes[offset])) {

    tree_free(mutated_tree);
    return NULL;

  }

  picked_edge.subnode = picked_edge.parent->subnodes[offset];
  node_t *parent = picked_edge.parent;
  node_t *tail = picked_edge.subnode;

  // detach the tail
  tail->parent = NULL;
  parent->subnodes[offset] = NULL;

  node_t *cloned_part = NULL;
  for (size_t i = 0; i < num; ++i) {

    cloned_part = node_clone(parent);
    if (unlikely(!cloned_part)) {

      // Reattach the tail to `parent`, then drop the repetitions built so far.
      // `tail` is the last one built, or the original tail.
      if (tail != picked_edge.subnode) {

        tail->parent = NULL;
        parent->subnodes[offset] = picked_edge.subnode;
        picked_edge.subnode->parent = parent;
        // The chain from `tail` down to `picked_edge.subnode` is a copy.
        // Detach the original tail from its deepest copy before the free.
        node_t *cur = tail;
        while (cur->subnodes[offset] != picked_edge.subnode)
          cur = cur->subnodes[offset];
        cur->subnodes[offset] = NULL;
        node_free(tail);

      } else {

        picked_edge.subnode->parent = parent;
        parent->subnodes[offset] = picked_edge.subnode;

      }

      tree_free(mutated_tree);
      return NULL;

    }

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
  if (unlikely(!mutated_tree)) return NULL;

  // randomly pick a node in the tree
  node_t *node = node_pick_non_term_subnode(mutated_tree->root);
  if (unlikely(node == NULL)) {

    // The sizes of the tree are 0, for example after a failed size walk
    fprintf(stderr, "The tree has no node to pick (%s)\n", __func__);
    tree_free(mutated_tree);
    return NULL;

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
