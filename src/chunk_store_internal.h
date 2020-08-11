#ifndef __CHUNK_STORE_INTERNAL_H__
#define __CHUNK_STORE_INTERNAL_H__

#include "map.h"
#include "set.h"
#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef map_t(list_t *) list_map_t;
extern list_map_t chunk_store;
extern simple_set seen_chunks;

// private functions
uint8_t *buf_from_node(node_t *node);
void chunk_store_add_node(node_t *node);

#ifdef __cplusplus
}
#endif

#endif
