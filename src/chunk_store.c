#include "list.h"
#include "f1_c_fuzz.h"
#include "chunk_store.h"
#include "chunk_store_internal.h"

// the list, in `chunk_store`, contains a collection of `node_t`
list_map_t chunk_store = {NULL};
simple_set seen_chunks;

uint8_t *buf_from_node(node_t *node) {
  uint8_t *buf = NULL;
  tree_t * tree = tree_create();
  tree->root = node;
  tree_to_buf(tree);

  buf = (uint8_t *)maybe_grow(BUF_PARAMS(tree, data), tree->data_len + 1);
  buf[tree->data_len] = '\0';

  tree->data_buf = NULL;
  tree->data_len = 0;
  tree->data_size = 0;
  tree->root = NULL;

  tree_free(tree);

  return buf;
}

void chunk_store_add_node(node_t *node) {
  if (!node) return;
  if (node->id == 0) return;

  const char *node_type = node_type_str(node->id);

  // add current subtree
  char *node_buffer = (char *)buf_from_node(node);
  if (set_contains(&seen_chunks, node_buffer)) {
    node_t *_node = node_clone(node);
    set_add(&seen_chunks, node_buffer);

    list_t **p_node_list = map_get(&chunk_store, node_type);
    if (unlikely(!p_node_list)) {
      map_set(&chunk_store, node_type, list_create());
      p_node_list = map_get(&chunk_store, node_type);
    }
    list_t *node_list = *p_node_list;
    list_append(node_list, _node);
  }
  free(node_buffer);

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
