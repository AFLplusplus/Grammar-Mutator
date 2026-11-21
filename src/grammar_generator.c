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
#include <unistd.h>

#include "f1_c_fuzz.h"
#include "utils.h"
#include "version.h"

// The number of tries to find a new test case. If all tries give an empty test
// case or a test case that the generator already wrote, the generator stops.
#define MAX_TRIES 100

// Generate a test case that is not empty and is not in `seen`. Return NULL if
// MAX_TRIES tries do not give one, or if there is not sufficient memory. The
// second case sets `*out_of_memory`.
static tree_t *generate_new_tree(int max_len, hash_set_t *seen,
                                 bool *out_of_memory) {

  *out_of_memory = false;
  for (int i = 0; i < MAX_TRIES; ++i) {

    tree_t *tree = gen_init__(max_len);
    if (!tree_to_buf(tree)) {

      tree_free(tree);
      *out_of_memory = true;
      return NULL;

    }

    if (tree->data_len &&
        hash_set_add(seen, hash_bytes(tree->data_buf, tree->data_len)))
      return tree;
    tree_free(tree);

  }

  return NULL;

}

int main(int argc, const char *argv[]) {

  int         seed, max_num, max_len;
  const char *out_dir, *tree_out_dir;

  if (argc < 4) {

    printf("Grammar Mutator %s\n", GRAMMAR_MUTATOR_VERSION);
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

  // The generator writes no empty test case and no test case twice
  char        fn[PATH_MAX];
  tree_t     *tree = NULL;
  hash_set_t *seen = hash_set_create();
  int         num = 0;
  if (!seen) {

    perror("Unable to allocate memory");
    return EXIT_FAILURE;

  }

  for (; num < max_num; ++num) {

    bool out_of_memory;
    tree = generate_new_tree(max_len, seen, &out_of_memory);
    if (out_of_memory) {

      fprintf(stderr, "Unable to allocate memory for a test case\n");
      hash_set_free(seen);
      return EXIT_FAILURE;

    }

    if (!tree) {

      printf("No new test case after %d tries; stop at %d test cases\n",
             MAX_TRIES, num);
      break;

    }

    // Write the test case and its tree file. If the tree write fails, also
    // remove the test case and stop. Then no test case is without its tree
    // file.
    char tree_fn[PATH_MAX];
    snprintf(fn, PATH_MAX, "%s/%d", out_dir, num);
    snprintf(tree_fn, PATH_MAX, "%s/%d", tree_out_dir, num);
    bool written = dump_tree_to_test_case(tree, fn);
    if (written && !write_tree_to_file(tree, tree_fn)) {

      unlink(fn);
      written = false;

    }

    tree_free(tree);
    if (!written) {

      fprintf(stderr, "Unable to write test case %d; stop\n", num);
      hash_set_free(seen);
      return EXIT_FAILURE;

    }

  }

  hash_set_free(seen);
  printf("Wrote %d test cases\n", num);
  return 0;

}
