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

#include "xxh3.h"
#include "list.h"
#include "f1_c_fuzz.h"
#include "chunk_store.h"
#include "chunk_store_internal.h"

// the list, in `chunk_store`, contains a collection of `node_t`
list_map_t chunk_store;
simple_set seen_chunks;

// Tiny implementation of fixed-length hash to text conversion
static void uint64_to_hex(uint64_t num, char dest[16+1]) {

  // Starting at the end and working backward:
  int i;
  char * c = &dest[16];

  *c = '\0';
  for (i = 0; i < 16; ++i) {

    --c;
    *c = "0123456789ABCDEF"[num & 0xF];
    num >>= 4;

  }

}

// Append the node and its subnodes a hash of the node and its subnodes
static void node_update_hash(node_t *node, XXH3_state_t* hash) {

  // Use the same fields that `node_equal()` uses, so
  // that we can be reasonably certain that if the hashes
  // are equal than `node_equal()` will return true.
  XXH3_64bits_update(hash, &node->id, sizeof(node->id));
  XXH3_64bits_update(hash, &node->rule_id, sizeof(node->rule_id));
  XXH3_64bits_update(hash, &node->val_len, sizeof(node->val_len));
  XXH3_64bits_update(hash, &node->val_buf, node->val_len);

  // Do not consider the parent node while comparing two nodes

  // subnodes
  for (uint32_t i = 0; i < node->subnode_count; ++i) {

    node_update_hash(node->subnodes[i], hash);

  }

}

// Create a hash of the node and its subnodes
// Use the same fields that `node_equal()` uses, so
// that we can be reasonably certain that if the hashes
// are equal than `node_equal()` will return true.
void hash_node(node_t *node, char dest[16+1]) {

  // Set up a hash state so we can pass it to sub-nodes recursively:
  XXH3_state_t hash;
  XXH3_64bits_reset(&hash);

  node_update_hash(node, &hash);

  // Need to convert the hash to text so that 0-values in the hash don't cause an inordinant amount of collisions.
  // If we just put the 8-byte integer in as a "string" then the first byte being a zero would cause
  // a collision approximately 1/256 of the time!
  uint64_to_hex(XXH3_64bits_digest(&hash), dest);

}

void chunk_store_add_node(node_t *node) {

  if (!node) return;
  if (node->id == 0) return;

  const char *node_type = node_type_str(node->id);

  // add current subtree
  char node_hash[16+1];
  hash_node(node, node_hash);
  if (set_contains(&seen_chunks, node_hash) == SET_FALSE) {

    node_t *_node = node_clone(node);
    set_add(&seen_chunks, node_hash);

    list_t **p_node_list = map_get(&chunk_store, node_type);
    if (unlikely(!p_node_list)) {

      map_set(&chunk_store, node_type, list_create());
      p_node_list = map_get(&chunk_store, node_type);

    }

    list_t *node_list = *p_node_list;
    list_append(node_list, _node);

  }

  // process subnodes
  node_t *subnode = NULL;
  for (uint32_t i = 0; i < node->subnode_count; ++i) {

    subnode = node->subnodes[i];
    chunk_store_add_node(subnode);

  }

}

void chunk_store_init() {

  map_init(&chunk_store);
  set_init(&seen_chunks);

}

void chunk_store_add_tree(tree_t *tree) {

  if (!tree) return;
  chunk_store_add_node(tree->root);

}

node_t *chunk_store_get_alternative_node(node_t *node) {

  if (!node) return NULL;

  const char *node_type = node_type_str(node->id);
  list_t **   p_node_list = map_get(&chunk_store, node_type);
  if (unlikely(!p_node_list)) return NULL;

  list_t *node_list = *p_node_list;
  size_t  n = node_list->size;
  int     prob = random() % n;
  // must clone the node
  return node_clone((node_t *)list_get(node_list, prob));

}

void chunk_store_clear() {

  set_destroy(&seen_chunks);

  const char *key;
  map_iter_t  iter = map_iter(&list_map);
  while ((key = map_next(&chunk_store, &iter))) {

    list_free_with_data_free_func(*map_get(&chunk_store, key),
                                  (data_free_t)node_free);

  }

  map_deinit(&chunk_store);

}
