#include <map>
#include <set>

#include "list.h"
#include "chunk_store.h"
#include "chunk_store_internal.h"

using namespace std;

map<uint32_t, list_t *> chunk_store;
set<buffer>             seen_chunks;

void chunk_store_add_node(node_t *node) {
  if (!node) return;
  if (node->id == 0) return;

  // add current subtree
  auto node_buffer = buffer(node);
  if (seen_chunks.find(node_buffer) == seen_chunks.end()) {
    node_t *_node = node_clone(node);
    seen_chunks.insert(node_buffer);

    list_t *node_list = chunk_store[_node->id];
    if (unlikely(!node_list)) {
      node_list = list_create();
      chunk_store[_node->id] = node_list;
    }
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
  if (chunk_store.find(node->id) == chunk_store.end()) return nullptr;

  list_t *node_list = chunk_store[node->id];
  size_t  n = node_list->size;
  int     prob = random() % n;
  // must clone the node
  return node_clone((node_t *)list_get(node_list, prob));
}

void chunk_store_clear() {
  seen_chunks.clear();

  for (const auto &pair : chunk_store) {
    list_free_with_data_free_func(pair.second, (data_free_t)node_free);
  }

  chunk_store.clear();
}
