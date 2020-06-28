#include "parsing_tree.h"

// AFL++
#include <list.h>

int main() {
  parsing_tree_t tree;
  node_t root = {0};

  // start -> json
  tree.root = &root;
  node_t json = {0};
  node_append_subnode(&root, &json);

  // json -> element
  node_t element = {0};
  node_append_subnode(&json, &element);

  // element -> ws_1, value, ws_2
  node_t ws_1 = {0}, value = {0}, ws_2 = {0};
  node_append_subnode(&element, &ws_1);
  node_append_subnode(&element, &value);
  node_append_subnode(&element, &ws_2);

  // ws_1 -> sp1_1, ws_3
  node_t sp1_1 = {0}, ws_3 = {0};
  node_append_subnode(&ws_1, &sp1_1);
  node_append_subnode(&ws_1, &ws_3);

  // sp1_1 == ' '
  sp1_1.val_size = 1;
  sp1_1.val_buf = malloc(sp1_1.val_size * sizeof(uint8_t));
  sp1_1.val_buf[0] = ' ';

  // ws_3 == null
  ws_3.val_size = 0;
  ws_3.val_buf = NULL;

  // value == "true"
  value.val_size = 4;
  value.val_buf = malloc(value.val_size * sizeof(uint8_t));
  memcpy(value.val_buf, "true", 4);

  // ws_2 == null
  ws_2.val_size = 0;
  ws_2.val_buf = NULL;

  tree_to_buf(&tree);

  if (memcmp(" true", tree.data_buf, tree.data_len) != 0) {
    fprintf(stderr, "%.*s\n", (int)tree.data_len, tree.data_buf);
    return 1;
  }

  return 0;
}
