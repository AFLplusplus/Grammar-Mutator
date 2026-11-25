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
#include <time.h>

#include "parse_budget.h"

struct parse_budget {

  // ring buffer of ns-per-token samples
  uint64_t samples[PARSE_BUDGET_WINDOW];
  // next write position in the ring buffer
  size_t next;
  // number of valid samples (at most PARSE_BUDGET_WINDOW)
  size_t filled;
  // number of records since the start
  size_t records;
  // the smallest limit in ns
  uint64_t min_ns;

};

parse_budget_t *parse_budget_create(void) {

  parse_budget_t *budget = (parse_budget_t *)calloc(1, sizeof(parse_budget_t));
  if (!budget) {

    perror("parse_budget_create (calloc)");
    return NULL;

  }

  budget->min_ns = PARSE_BUDGET_MIN_NS;
  return budget;

}

void parse_budget_set_min_ns(parse_budget_t *budget, uint64_t min_ns) {

  if (budget) budget->min_ns = min_ns;

}

void parse_budget_free(parse_budget_t *budget) {

  free(budget);

}

static int compare_u64(const void *a, const void *b) {

  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return (x > y) - (x < y);

}

uint64_t parse_budget_limit_ns(const parse_budget_t *budget, size_t tokens) {

  if (!budget || budget->records < PARSE_BUDGET_WARMUP) return 0;

  // sort a copy and take the middle value
  uint64_t sorted[PARSE_BUDGET_WINDOW];
  memcpy(sorted, budget->samples, budget->filled * sizeof(uint64_t));
  qsort(sorted, budget->filled, sizeof(uint64_t), compare_u64);
  uint64_t median = sorted[budget->filled / 2];

  // limit = factor * median * tokens, saturated
  uint64_t count = tokens ? (uint64_t)tokens : 1;
  uint64_t limit;
  if (median > UINT64_MAX / PARSE_BUDGET_FACTOR) {

    limit = UINT64_MAX;

  } else {

    uint64_t per_token = median * PARSE_BUDGET_FACTOR;
    limit = per_token > UINT64_MAX / count ? UINT64_MAX : per_token * count;

  }

  return limit < budget->min_ns ? budget->min_ns : limit;

}

void parse_budget_record(parse_budget_t *budget, uint64_t elapsed_ns,
                         size_t tokens) {

  if (!budget) return;
  uint64_t count = tokens ? (uint64_t)tokens : 1;
  budget->samples[budget->next] = elapsed_ns / count;
  budget->next = (budget->next + 1) % PARSE_BUDGET_WINDOW;
  if (budget->filled < PARSE_BUDGET_WINDOW) ++budget->filled;
  if (budget->records < PARSE_BUDGET_WARMUP) ++budget->records;

}

uint64_t parse_budget_now_ns(void) {

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;

}
