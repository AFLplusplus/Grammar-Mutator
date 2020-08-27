/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Written by Shengtuo Hu

   Copyright 2020 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

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
  const char *out_dir, *tree_out_dir;
//  size_t      out_dir_len, tree_out_dir_len;

  if (argc < 5) {
    printf("%s <random seed> <max_num> <max_size> <output_dir> <tree_output_dir>\n",
           argv[0]);
    return 0;
  }

  seed = atoi(argv[1]);
  max_num = atoi(argv[2]);
  max_len = atoi(argv[3]);
  out_dir = argv[4];
//  out_dir_len = strlen(out_dir);
  tree_out_dir = argv[5];
//  tree_out_dir_len = strlen(tree_out_dir);

  srandom(seed);

  struct stat info;
  if (stat(out_dir, &info) != 0) {
    // The output directory does not exist
    if (mkdir(out_dir, 0700) != 0) {
      // error
      perror("Cannot create the output directory");
      return EXIT_FAILURE;
    }
  } else if (info.st_mode & S_IFDIR) {
    // Exist
  } else {
    // Not a directory
    perror("Wrong output path (stat)");
    return EXIT_FAILURE;
  }

  if (stat(tree_out_dir, &info) != 0) {
    // The output directory does not exist
    if (mkdir(tree_out_dir, 0700) != 0) {
      // error
      perror("Cannot create the tree output directory");
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
  for (int i = 0; i < max_num; ++i) {
    tree = gen_init__(max_len);

    snprintf(fn, PATH_MAX, "%s/%d", out_dir, i);
    dump_tree_to_test_case(tree, fn);
    snprintf(fn, PATH_MAX, "%s/id:%06d,time:0,orig:%d", tree_out_dir, i, i);
    write_tree_to_file(tree, fn);

    tree_free(tree);
  }

  return 0;
}
