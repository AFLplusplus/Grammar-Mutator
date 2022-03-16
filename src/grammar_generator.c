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
#include <time.h>

#include "f1_c_fuzz.h"
#include "utils.h"

int main(int argc, const char *argv[]) {

  int         seed, max_num, max_len;
  const char *out_dir, *tree_out_dir;

  if (argc < 4) {

    printf(
        "%s <max_num> <max_size> <output_dir> <tree_output_dir> [<random "
        "seed>]\n",
        argv[0]);
    return 0;

  }

  max_num = atoi(argv[1]);
  max_len = atoi(argv[2]);
  out_dir = argv[3];
  tree_out_dir = argv[4];
  if (argc > 5)
    seed = atoi(argv[5]);
  else
    seed = (int)time(NULL);

  printf("Using seed %d\n", seed);
  random_set_seed((uint64_t)seed);

  if (!create_directory(out_dir)) {

    fprintf(stderr, "Cannot create the output directory\n");
    return EXIT_FAILURE;

  }

  if (!create_directory(tree_out_dir)) {

    fprintf(stderr, "Cannot create the tree output directory\n");
    return EXIT_FAILURE;

  }

  char    fn[PATH_MAX];
  tree_t *tree = NULL;
  for (int i = 0; i < max_num; ++i) {

    tree = gen_init__(max_len);

    snprintf(fn, PATH_MAX, "%s/%d", out_dir, i);
    dump_tree_to_test_case(tree, fn);
    snprintf(fn, PATH_MAX, "%s/%d", tree_out_dir, i);
    write_tree_to_file(tree, fn);

    tree_free(tree);

  }

  return 0;

}
