#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

#include "f1_c_fuzz.h"

static void dump_test_case(uint8_t *buf, size_t buf_size) {
  fprintf(stdout, "%.*s\n", (int)buf_size, buf);
}

int main(int argc, const char *argv[]) {
  int         seed, max_num, max_len;
  const char *out_dir;
  size_t      out_dir_len;

  if (argc < 4) {
    printf("%s <seed> <max_num> <max_len> <output_dir>\n", argv[0]);
    return 0;
  }

  seed = atoi(argv[1]);
  max_num = atoi(argv[2]);
  max_len = atoi(argv[3]);
  out_dir = argv[4];
  out_dir_len = strlen(out_dir);

  srandom(seed);

  struct stat info;
  if (stat(out_dir, &info) != 0) {
    // The output directory does not exist
    if (mkdir(out_dir, 0700) != 0) {
      // error
      perror("Cannot create the directory");
      return EXIT_FAILURE;
    }
  } else if (info.st_mode & S_IFDIR) {
    // Exist
  } else {
    // Not a directory
    perror("Wrong tree output path (stat)");
    return EXIT_FAILURE;
  }

  char    fn[PATH_MAX];
  tree_t *tree = NULL;
  strncpy(fn, out_dir, out_dir_len);
  for (int i = 0; i < max_num; ++i) {
    tree = gen_init__(max_len);

    snprintf(fn + out_dir_len, PATH_MAX - out_dir_len, "/%d", i);
    dump_tree_to_test_case(tree, fn);
    snprintf(fn + out_dir_len, PATH_MAX - out_dir_len, "/%d.tree", i);
    write_tree_to_file(tree, fn);

    tree_free(tree);
  }

  return 0;
}
