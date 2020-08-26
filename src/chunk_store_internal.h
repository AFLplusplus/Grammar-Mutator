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
size_t buf_from_node(node_t *node, uint8_t **out_buf);
void   hash_node(node_t *node, char dest[9]);
void   chunk_store_add_node(node_t *node);

#ifdef __cplusplus
}
#endif

#endif
