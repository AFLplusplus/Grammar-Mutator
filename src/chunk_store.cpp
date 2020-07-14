#include <map>
#include <set>
#include <vector>

#include "chunk_store.h"
#include "chunk_store_internal.h"

using namespace std;

map<uint32_t, vector<node_t *>> chunk_store;
set<buffer>                     seen_chunks;

void chunk_store_add_node(node_t *node) {
  if (!node) return;
  if (node->id == 0) return;

  // add current subtree
  auto node_buffer = buffer(node);
  if (seen_chunks.find(node_buffer) == seen_chunks.end()) {
    node_t *_node = node_clone(node);
    seen_chunks.insert(node_buffer);
    chunk_store[_node->id].push_back(_node);
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

  size_t n = chunk_store[node->id].size();
  int    prob = random() % n;
  return chunk_store[node->id][prob];
}

void chunk_store_clear() {
  seen_chunks.clear();

  for (const auto &pair : chunk_store) {
    for (auto &node : chunk_store[pair.first]) {
      node_free(node);
    }
  }

  chunk_store.clear();
}
