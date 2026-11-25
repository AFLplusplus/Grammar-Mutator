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

#define XXH_INLINE_ALL
#include "xxhash.h"
#include "list.h"
#include "f1_c_fuzz.h"
#include "chunk_store.h"
#include "chunk_store_internal.h"
#include "utils.h"
#include "node_stack.h"

// the list, in `chunk_store`, contains a collection of `node_t`
list_map_t chunk_store;
node_map_t seen_chunks;

// Instrumentation for tests: the number of nodes that the hash passes visit
size_t chunk_store_hashed_nodes = 0;

// Tiny implementation of fixed-length hash to text conversion
static void uint64_to_hex(uint64_t num, char dest[16 + 1]) {

  // Starting at the end and working backward:
  int   i;
  char *c = &dest[16];

  *c = '\0';
  for (i = 0; i < 16; ++i) {

    --c;
    *c = "0123456789ABCDEF"[num & 0xF];
    num >>= 4;

  }

}

// One entry for each node of a subtree, in pre-order
typedef struct {

  node_t  *node;
  size_t   parent;  // the entry index of the parent
  uint32_t idx;     // the child index in the parent
  size_t   base;    // the first slot of the child hashes
  size_t   size;    // the number of entries in the subtree, itself included
  uint64_t hash;

} hash_entry_t;

// Hash every node of the subtree in one post-order pass. The hash of a node
// covers the same fields that `node_equal()` uses and the hashes of its
// children. The caller frees the result. Return NULL, and set `*count` to 0,
// if memory runs out.
static hash_entry_t *hash_subtree(node_t *root, size_t *count) {

  hash_entry_t *entries = NULL;
  size_t        n = 0, cap = 0, slots = 0;
  node_stack_t  stack = {0};

  *count = 0;
  if (!node_stack_push(&stack, root, 0, (size_t)-1)) return NULL;
  while (stack.len) {

    node_frame_t f = node_stack_pop(&stack);
    if (n == cap) {

      size_t        new_cap = cap ? cap * 2 : 64;
      hash_entry_t *new_entries =
          realloc(entries, new_cap * sizeof(hash_entry_t));
      if (!new_entries) {

        perror("hash_subtree (realloc)");
        goto fail;

      }

      entries = new_entries;
      cap = new_cap;

    }

    entries[n] = (hash_entry_t){f.node, f.aux, f.idx, slots, 1, 0};
    slots += f.node->subnode_count;
    ++n;
    if (!node_stack_push_children(&stack, f.node, n - 1)) goto fail;

  }

  node_stack_free(&stack);

  // child hashes: a NULL subnode keeps the slot value 0
  uint64_t *child_hash = calloc(slots ? slots : 1, sizeof(uint64_t));
  if (!child_hash) {

    perror("hash_subtree (calloc)");
    free(entries);
    return NULL;

  }

  chunk_store_hashed_nodes += n;

  // One hash state serves every node; the reset below makes it fresh.
  XXH3_state_t hash;

  // The reverse pre-order has every child before its parent
  for (size_t i = n; i-- > 0;) {

    node_t *node = entries[i].node;

    // Use the same fields that `node_equal()` uses, so
    // that we can be reasonably certain that if the hashes
    // are equal than `node_equal()` will return true.
    // Do not consider the parent node while comparing two nodes.
    XXH3_64bits_reset(&hash);
    XXH3_64bits_update(&hash, &node->id, sizeof(node->id));
    XXH3_64bits_update(&hash, &node->rule_id, sizeof(node->rule_id));
    XXH3_64bits_update(&hash, &node->val_len, sizeof(node->val_len));
    XXH3_64bits_update(&hash, node->val_buf, node->val_len);
    XXH3_64bits_update(&hash, child_hash + entries[i].base,
                       node->subnode_count * sizeof(uint64_t));
    entries[i].hash = XXH3_64bits_digest(&hash);

    if (i) {

      size_t p = entries[i].parent;
      child_hash[entries[p].base + entries[i].idx] = entries[i].hash;
      entries[p].size += entries[i].size;

    }

  }

  free(child_hash);
  *count = n;
  return entries;

fail:
  node_stack_free(&stack);
  free(entries);
  return NULL;

}

// Create a hash of the node and its subnodes
// Use the same fields that `node_equal()` uses, so
// that we can be reasonably certain that if the hashes
// are equal than `node_equal()` will return true.
void hash_node(node_t *node, char dest[16 + 1]) {

  size_t        count;
  hash_entry_t *entries = hash_subtree(node, &count);
  if (!entries) {

    // Out of memory. Give an all-zero hash.
    memset(dest, '0', 16);
    dest[16] = '\0';
    return;

  }

  // Need to convert the hash to text so that 0-values in the hash don't cause
  // an inordinant amount of collisions. If we just put the 8-byte integer in as
  // a "string" then the first byte being a zero would cause a collision
  // approximately 1/256 of the time!
  uint64_to_hex(entries[0].hash, dest);
  free(entries);

}

/**
 * Take ownership of a node for the chunk store, storing it if unique or freeing
 * it if not.
 * @param  node The node, which must not be owned/kept by anybody else. It also
 * may have a parent, whose subnode pointer the function patches.
 */
void chunk_store_take_node(node_t *node) {

  if (!node) return;

  size_t        count;
  hash_entry_t *entries = hash_subtree(node, &count);
  if (!entries) {

    // Out of memory: the store cannot keep the node. Detach and free it.
    if (node->parent) {

      for (uint32_t i = 0; i < node->parent->subnode_count; ++i)
        if (node->parent->subnodes[i] == node) node->parent->subnodes[i] = NULL;

    }

    node_free(node);
    return;

  }

  // Walk the entries in pre-order. A duplicate subtree is freed, so skip its
  // entries.
  for (size_t i = 0; i < count;) {

    node_t *cur = entries[i].node;
    char    node_hash[16 + 1];
    uint64_to_hex(entries[i].hash, node_hash);

    node_t **seen_node = map_get(&seen_chunks, node_hash);
    if (seen_node) {

      // This node already exists in the tree. So patch up our parent to point
      // at it and then free the duplicate.
      if (cur->parent) node_replace_subnode(cur->parent, cur, *seen_node);

      // We're a duplicate and not needed anymore, and neither are our subnodes!
      size_t skip = entries[i].size;
      node_free(cur);
      i += skip;
      continue;

    }

    // This is a brand new node, so keep it!
    map_set(&seen_chunks, node_hash, cur);

    // NOTE: If this is a terminal node (node->id == 0), we *could*
    // skip creating a list for them here, because they aren't usable
    // for the splicing mutation (because they are not all interchangable).
    // But for simplicity we can just leave it as-is for now.
    const char *node_type = node_type_str(cur->id);
    list_t    **p_node_list = map_get(&chunk_store, node_type);
    if (unlikely(!p_node_list)) {

      map_set(&chunk_store, node_type, list_create());
      p_node_list = map_get(&chunk_store, node_type);

    }

    list_append(*p_node_list, cur);
    ++i;

  }

  free(entries);

}

void chunk_store_init() {

  map_init(&chunk_store);
  map_init(&seen_chunks);

}

void chunk_store_add_tree(tree_t *tree) {

  if (!tree || !tree->root) return;

  // A known tree adds nothing, so check its hash before the copy. A new tree
  // is hashed again when the store takes the copy.
  size_t        count;
  hash_entry_t *entries = hash_subtree(tree->root, &count);
  if (entries) {

    char root_hash[16 + 1];
    uint64_to_hex(entries[0].hash, root_hash);
    bool known = map_get(&seen_chunks, root_hash) != NULL;
    free(entries);
    if (known) return;

  }

  // Clone the tree and then hand it off to the chunk_store
  chunk_store_take_node(node_clone(tree->root));

}

node_t *chunk_store_get_alternative_node(node_t *node) {

  if (!node) return NULL;

  const char *node_type = node_type_str(node->id);
  list_t    **p_node_list = map_get(&chunk_store, node_type);
  if (unlikely(!p_node_list)) return NULL;

  list_t *node_list = *p_node_list;
  size_t  n = node_list->size;
  int     prob = random_below(n);
  // must clone the node
  return node_clone((node_t *)list_get(node_list, prob));

}

void chunk_store_clear() {

  map_deinit(&seen_chunks);

  const char *key;
  map_iter_t  iter = map_iter(&list_map);
  while ((key = map_next(&chunk_store, &iter))) {

    // NOTE: This needs to only free the CURRENT node and not its children,
    // because we know that the
    //       chunk store map already contains all the children and they will get
    //       freed as well!
    list_free_with_data_free_func(*map_get(&chunk_store, key),
                                  (data_free_t)node_free_only_self);

  }

  map_deinit(&chunk_store);

}
