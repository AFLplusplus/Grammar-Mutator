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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>

#include "f1_c_fuzz.h"
#include "utils.h"

// The number of tries to find a new test case. If all tries give an empty test
// case or a test case that the generator already wrote, the generator stops.
#define MAX_TRIES 100

// A set of 64-bit hashes (open addressing). The value 0 marks a free slot.
typedef struct hash_set {

  uint64_t *slots;
  size_t    capacity;
  size_t    count;

} hash_set_t;

// Return the FNV-1a hash of the bytes (never 0)
static uint64_t hash_bytes(const uint8_t *buf, size_t len) {

  uint64_t hash = 0xcbf29ce484222325ULL;
  for (size_t i = 0; i < len; ++i) {

    hash ^= buf[i];
    hash *= 0x100000001b3ULL;

  }

  return hash ? hash : 1;

}

static void hash_set_insert(hash_set_t *set, uint64_t hash) {

  size_t i = hash & (set->capacity - 1);
  while (set->slots[i] && set->slots[i] != hash)
    i = (i + 1) & (set->capacity - 1);
  if (!set->slots[i]) {

    set->slots[i] = hash;
    ++set->count;

  }

}

// Add the hash to the set. Return false if the set already has it.
static bool hash_set_add(hash_set_t *set, uint64_t hash) {

  if (set->count * 2 >= set->capacity) {

    // Keep the load below one half
    hash_set_t bigger = {calloc(set->capacity * 2, sizeof(uint64_t)),
                         set->capacity * 2, 0};
    if (!bigger.slots) {

      perror("Unable to allocate memory (hash_set_add)");
      exit(EXIT_FAILURE);

    }

    for (size_t i = 0; i < set->capacity; ++i)
      if (set->slots[i]) hash_set_insert(&bigger, set->slots[i]);
    free(set->slots);
    *set = bigger;

  }

  size_t old_count = set->count;
  hash_set_insert(set, hash);
  return set->count != old_count;

}

// Generate a test case that is not empty and is not in `seen`. Return NULL if
// MAX_TRIES tries do not give one.
static tree_t *generate_new_tree(int max_len, hash_set_t *seen) {

  for (int i = 0; i < MAX_TRIES; ++i) {

    tree_t *tree = gen_init__(max_len);
    tree_to_buf(tree);
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
  char       fn[PATH_MAX];
  tree_t    *tree = NULL;
  hash_set_t seen = {calloc(64, sizeof(uint64_t)), 64, 0};
  int        num = 0;
  if (!seen.slots) {

    perror("Unable to allocate memory");
    return EXIT_FAILURE;

  }

  for (; num < max_num; ++num) {

    tree = generate_new_tree(max_len, &seen);
    if (!tree) {

      printf("No new test case after %d tries; stop at %d test cases\n",
             MAX_TRIES, num);
      break;

    }

    snprintf(fn, PATH_MAX, "%s/%d", out_dir, num);
    dump_tree_to_test_case(tree, fn);
    snprintf(fn, PATH_MAX, "%s/%d", tree_out_dir, num);
    write_tree_to_file(tree, fn);

    tree_free(tree);

  }

  free(seen.slots);
  printf("Wrote %d test cases\n", num);
  return 0;

}
