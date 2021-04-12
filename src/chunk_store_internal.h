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

#ifndef __CHUNK_STORE_INTERNAL_H__
#define __CHUNK_STORE_INTERNAL_H__

#include "map.h"
#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

// Map of lists of chunks, one list for each node type
typedef map_t(list_t *) list_map_t;
extern list_map_t chunk_store;

// Map of node hashes to pointers to stored nodes
// in the chunk_store. This can quickly identify if
// a node already exists in the chunk_store, by searching
// this map for an existing hash.
typedef map_t(node_t *) node_map_t;
extern node_map_t seen_chunks;

// private functions
void   hash_node(node_t *node, char dest[16+1]);
void   chunk_store_take_node(node_t *node);

#ifdef __cplusplus
}
#endif

#endif
