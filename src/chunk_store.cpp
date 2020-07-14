#include <map>
#include <set>
#include <vector>

#include "chunk_store.h"

using namespace std;

struct buffer {
  BUF_VAR(uint8_t, data);
  size_t data_len;

  buffer() {
    data_buf = nullptr;
    data_size = 0;
    data_len = 0;
  }

  explicit buffer(node_t *node) : buffer() {
    from_node(node);
  }

  buffer(const buffer &_other) : buffer() {
    if (data_len == 0) return;

    data_len = _other.data_len;
    data_size = _other.data_size;
    data_buf = (uint8_t *)realloc(data_buf, data_size);
    memcpy(data_buf, _other.data_buf, data_len);
  }

  ~buffer() {
    if (data_buf) {
      free(data_buf);
      data_buf = nullptr;
      data_size = 0;
      data_len = 0;
    }
  }

  void from_node(node_t *node) {
    if (!node) return;

    // dump `val` if this is a leaf node
    if (node->subnode_count == 0) {
      if (node->val_len == 0) return;

      maybe_grow(BUF_PARAMS(this, data), data_len + node->val_len);
      if (!data_buf) {
        perror("tree output buffer allocation (maybe_grow)");
        return;
      }

      memcpy(data_buf + data_len, node->val_buf, node->val_len);
      data_len += node->val_len;

      return;
    }

    // subnodes
    node_t *subnode = nullptr;
    for (int i = 0; i < node->subnode_count; ++i) {
      subnode = node->subnodes[i];
      from_node(subnode);
    }
  }

  bool operator<(const buffer &rhs) const {
    if (data_len < rhs.data_len)
      return true;
    else if (data_len > rhs.data_len)
      return false;
    else
      return memcmp(data_buf, rhs.data_buf, data_len);
  }
};

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
