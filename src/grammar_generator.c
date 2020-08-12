#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "f1_c_fuzz.h"

static void dump_test_case(uint8_t *buf, size_t buf_size) {
  fprintf(stderr, "%.*s\n", (int)buf_size, buf);
}

int main(int argc, const char *argv[]) {
  int seed, max_num;

  if (argc < 3) {
    printf("%s <seed> <max_num> <max_depth>\\n", argv[0]);
    return 0;
  }

  seed = atoi(argv[1]);
  max_num = atoi(argv[2]);
  max_depth = atoi(argv[3]);

  srandom(seed);

  tree_t *tree = NULL;
  for (int i = 0; i < max_num; ++i) {
    tree = gen_init__();

    tree_to_buf(tree);
    tree_get_non_terminal_nodes(tree);

    fprintf(stderr, "=====%d=====\n", i + 1);
    fprintf(stderr, "Buffer size: %zu\n", tree->data_len);
    fprintf(stderr, "Tree #non-terminal nodes: %zu\n",
            tree->non_terminal_node_list->size);
    dump_test_case(tree->data_buf, tree->data_len);
    tree_free(tree);

    // TODO: the generated tree, using F1 fuzzer, is often shorter and simpler
    //  than `nautilus` outputs. Therefore, `libgrammarmutator` does not lead to
    //  high coverage. This may be due to the randomness of F1 fuzzer while
    //  generating each subtree. Sometimes, even if we set `max_depth` to 50 or
    //  higher, the generated tree may only have two non-terminal nodes.
    //  However, `nautilus` takes the size of the subtree into consideration.
    //  In this case, the generated tree has the maximal tree size within the
    //  constraint of the max allowed tree size (i.e., `max_depth`)
    //
    // Reference:
    // https://github.com/nautilus-fuzz/nautilus/blob/master/grammartec/src/rule.rs#L306
  }

  // TODO: write generated trees and corresponding buffer into files

  return 0;
}
