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
#include <time.h>
#include <unistd.h>

#include "tree.h"

// The helpers below are for the run and digest modes, which come later.
#define MAYBE_UNUSED __attribute__((unused))

// The time of a monotonic clock in microseconds
MAYBE_UNUSED static double now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

// The grammar name: the text after "benchmark-" in the base name of argv0
MAYBE_UNUSED static const char *grammar_name(const char *argv0) {
  const char *base = strrchr(argv0, '/');
  base = base ? base + 1 : argv0;
  static const char prefix[] = "benchmark-";
  if (strncmp(base, prefix, sizeof(prefix) - 1) != 0) return "unknown";
  base += sizeof(prefix) - 1;
  return *base ? base : "unknown";
}

// Print a value so that it has no white space
MAYBE_UNUSED static void print_value(FILE *out, const char *s) {
  for (; *s; ++s) {
    bool blank = *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n';
    fputc(blank ? '?' : *s, out);
  }
}

// The number of all nodes in a subtree
static size_t count_nodes(const node_t *node) {
  size_t count = 1;
  for (uint32_t i = 0; i < node->subnode_count; ++i)
    count += count_nodes(node->subnodes[i]);
  return count;
}

// Read a whole number: only digits, not empty, no overflow, at most max
MAYBE_UNUSED static bool parse_count(const char *s, uint64_t max,
                                     uint64_t *out) {
  if (*s == '\0') return false;
  uint64_t value = 0;
  for (; *s; ++s) {
    if (*s < '0' || *s > '9') return false;
    uint64_t digit = (uint64_t)(*s - '0');
    if (value > (UINT64_MAX - digit) / 10) return false;
    value = value * 10 + digit;
  }
  if (value > max) return false;
  *out = value;
  return true;
}

static int usage(const char *prog) {
  fprintf(stderr,
          "Usage:\n"
          "  %s parse FILE...\n"
          "  %s run    [-s SEED] [-e ENTRIES] [-n STEPS] [-t TRIMS] SEED_DIR\n"
          "  %s digest [-s SEED] [-n STEPS] [-t TRIMS] SEED_DIR\n",
          prog, prog, prog);
  return 2;
}

// Read a whole file. Return NULL on an error.
static uint8_t *read_file(const char *path, size_t *len) {
  FILE *fp = fopen(path, "rb");
  if (!fp) return NULL;
  size_t   cap = 4096;
  size_t   used = 0;
  uint8_t *buf = malloc(cap);
  while (buf) {
    size_t got = fread(buf + used, 1, cap - used, fp);
    used += got;
    if (used < cap) break;
    cap *= 2;
    uint8_t *bigger = realloc(buf, cap);
    if (!bigger) free(buf);
    buf = bigger;
  }
  bool bad = !buf || ferror(fp);
  fclose(fp);
  if (bad) {
    free(buf);
    return NULL;
  }
  *len = used;
  return buf;
}

// The parse mode: print the parse time of each file
static int mode_parse(int argc, char **argv) {
  int status = 0;
  for (int i = 0; i < argc; ++i) {
    size_t   len = 0;
    uint8_t *buf = read_file(argv[i], &len);
    printf("parse file=");
    print_value(stdout, argv[i]);
    if (!buf) {
      printf(" bytes=0 nodes=0 us=0.0 result=error\n");
      status = 1;
      continue;
    }
    double  begin = now_us();
    tree_t *tree = tree_from_buf(buf, len);
    double  elapsed = now_us() - begin;
    size_t  nodes = tree ? count_nodes(tree->root) : 0;
    printf(" bytes=%zu nodes=%zu us=%.1f result=%s\n", len, nodes, elapsed,
           tree ? "ok" : "fail");
    if (tree) tree_free(tree);
    free(buf);
  }
  return status;
}

int main(int argc, char **argv) {
  if (argc < 2) return usage(argv[0]);
  const char *mode = argv[1];
  if (strcmp(mode, "parse") == 0) {
    if (argc < 3) return usage(argv[0]);
    int status = mode_parse(argc - 2, argv + 2);
    fflush(stdout);
    return status;
  }
  // The run and digest modes come later.
  return usage(argv[0]);
}
