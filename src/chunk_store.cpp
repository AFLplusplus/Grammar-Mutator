#include <set>

#include "list.h"
#include "f1_c_fuzz.h"
#include "chunk_store.h"
#include "chunk_store_internal.h"

using namespace std;

// the list, in `chunk_store`, contains a collection of `node_t`
list_map_t  chunk_store = {nullptr};
set<buffer> seen_chunks;

void chunk_store_add_node(node_t *node) {
  if (!node) return;
  if (node->id == 0) return;

  const char *node_type = node_type_str(node->id);

  // add current subtree
  auto node_buffer = buffer(node);
  if (seen_chunks.find(node_buffer) == seen_chunks.end()) {
    node_t *_node = node_clone(node);
    seen_chunks.insert(node_buffer);

    list_t **p_node_list = map_get(&chunk_store, node_type);
    if (unlikely(!p_node_list)) {
      map_set(&chunk_store, node_type, list_create());
      p_node_list = map_get(&chunk_store, node_type);
    }
    list_t *node_list = *p_node_list;
    list_append(node_list, _node);
  }

  // process subnodes
  node_t *subnode = nullptr;
  for (int i = 0; i < node->subnode_count; ++i) {
    subnode = node->subnodes[i];
    chunk_store_add_node(subnode);
  }
}

void chunk_store_add_tree(tree_t *tree) {
  if (!tree) return;
  chunk_store_add_node(tree->root);
}

node_t *chunk_store_get_alternative_node(node_t *node) {
  if (!node) return nullptr;

  const char *node_type = node_type_str(node->id);
  list_t **   p_node_list = map_get(&chunk_store, node_type);
  if (unlikely(!p_node_list)) return nullptr;

  list_t *node_list = *p_node_list;
  size_t  n = node_list->size;
  int     prob = random() % n;
  // must clone the node
  return node_clone((node_t *)list_get(node_list, prob));
}

void chunk_store_clear() {
  seen_chunks.clear();

  const char *key;
  map_iter_t iter = map_iter(&list_map);
  while ((key = map_next(&chunk_store, &iter))) {
    list_free_with_data_free_func(*map_get(&chunk_store, key), (data_free_t)node_free);
  }

  map_deinit(&chunk_store);
}
