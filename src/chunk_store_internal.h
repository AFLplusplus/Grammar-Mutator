#ifndef __CHUNK_STORE_INTERNAL_H__
#define __CHUNK_STORE_INTERNAL_H__

#include "map.h"
#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef map_t(list_t *) list_map_t;
extern list_map_t chunk_store;

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
    if (_other.data_len == 0) return;

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
    size_t min_len = std::min(data_len, rhs.data_len);
    int    i = 0;
    for (; i < min_len; ++i) {
      if (data_buf[i] < rhs.data_buf[i]) return true;
      if (rhs.data_buf[i] < data_buf[i]) return false;
    }
    return (i == data_len) && (i != rhs.data_len);
  }

  bool operator==(const buffer &rhs) const {
    if (data_len != rhs.data_len) return false;
    return memcmp(data_buf, rhs.data_buf, data_len) == 0;
  }
};

#ifdef __cplusplus
}
#endif

#endif
