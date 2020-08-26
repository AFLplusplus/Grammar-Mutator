#include "hash.h"
#include "list.h"
#include "f1_c_fuzz.h"
#include "chunk_store.h"
#include "chunk_store_internal.h"

// the list, in `chunk_store`, contains a collection of `node_t`
list_map_t chunk_store = {NULL};
simple_set seen_chunks;

// temporary tree
static tree_t *temp_tree = NULL;

size_t buf_from_node(node_t *node, uint8_t **out_buf) {
  size_t out_buf_len = 0;

  temp_tree->root = node;
  tree_to_buf(temp_tree);

  *out_buf =
      (uint8_t *)maybe_grow(BUF_PARAMS(temp_tree, data), temp_tree->data_len);
  out_buf_len = temp_tree->data_len;

  temp_tree->data_buf = NULL;
  temp_tree->data_len = 0;
  temp_tree->data_size = 0;
  temp_tree->root = NULL;

  return out_buf_len;
}

void hash_node(node_t *node, char dest[9]) {
  uint8_t *node_buf = NULL;
  size_t   node_buf_len = buf_from_node(node, &node_buf);
  uint64_t node_hash = hash64(node_buf, node_buf_len, HASH_SEED);
  free(node_buf);

  memcpy(dest, &node_hash, 8);
  dest[8] = '\0';
}

void chunk_store_add_node(node_t *node) {
  if (!node) return;
  if (node->id == 0) return;

  const char *node_type = node_type_str(node->id);

  // add current subtree
  char node_hash[9];
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
  for (int i = 0; i < node->subnode_count; ++i) {
    subnode = node->subnodes[i];
    chunk_store_add_node(subnode);
  }
}

void chunk_store_init() {
  map_init(&chunk_store);
  set_init(&seen_chunks);

  temp_tree = tree_create();
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

  tree_free(temp_tree);
}
