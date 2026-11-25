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

#include <cstdint>
#include "gtest/gtest.h"
#include "parse_budget.h"

static parse_budget_t *fill(int count, uint64_t ns_per_token) {

  parse_budget_t *b = parse_budget_create();
  for (int i = 0; i < count; ++i)
    parse_budget_record(b, ns_per_token * 10, 10);
  return b;

}

TEST(ParseBudget, WarmUpHasNoLimit) {

  parse_budget_t *b = fill(PARSE_BUDGET_WARMUP - 1, 1000);
  EXPECT_EQ(parse_budget_limit_ns(b, 1000), 0u);
  parse_budget_record(b, 10000, 10);
  EXPECT_NE(parse_budget_limit_ns(b, 1000), 0u);
  parse_budget_free(b);

}

TEST(ParseBudget, LimitIsFactorTimesMedianTimesTokens) {

  parse_budget_t *b = fill(32, 1000);
  EXPECT_EQ(parse_budget_limit_ns(b, 10000), 50ull * 1000 * 10000);
  parse_budget_free(b);

}

TEST(ParseBudget, LimitHasAMinimum) {

  parse_budget_t *b = fill(32, 1);
  EXPECT_EQ(parse_budget_limit_ns(b, 10), 50000000u);
  parse_budget_free(b);

}

TEST(ParseBudget, SetMinNsSetsTheFloor) {

  parse_budget_t *b = fill(32, 1);
  EXPECT_EQ(parse_budget_limit_ns(b, 10), 50000000u);
  parse_budget_set_min_ns(b, 200000000);
  EXPECT_EQ(parse_budget_limit_ns(b, 10), 200000000u);
  // The floor does not cut a larger limit
  EXPECT_EQ(parse_budget_limit_ns(b, 10000000), 50ull * 1 * 10000000);
  parse_budget_set_min_ns(nullptr, 1);
  parse_budget_free(b);

}

TEST(ParseBudget, MedianIgnoresOneOutlier) {

  parse_budget_t *b = fill(31, 1000);
  parse_budget_record(b, 1000000000ull * 10, 10);
  EXPECT_EQ(parse_budget_limit_ns(b, 10000), 50ull * 1000 * 10000);
  parse_budget_free(b);

}

TEST(ParseBudget, WindowKeepsTheLast255) {

  parse_budget_t *b = fill(255, 1000000);
  for (int i = 0; i < 255; ++i)
    parse_budget_record(b, 1000 * 10, 10);
  EXPECT_EQ(parse_budget_limit_ns(b, 10000), 50ull * 1000 * 10000);
  parse_budget_free(b);

}

TEST(ParseBudget, ZeroTokensCountAsOne) {

  parse_budget_t *b = parse_budget_create();
  for (int i = 0; i < 32; ++i)
    parse_budget_record(b, 5000, 0);
  EXPECT_EQ(parse_budget_limit_ns(b, 1000000), 50ull * 5000 * 1000000);
  parse_budget_free(b);

}

TEST(ParseBudget, LimitSaturates) {

  parse_budget_t *b = fill(32, UINT64_MAX / 10);
  EXPECT_EQ(parse_budget_limit_ns(b, SIZE_MAX), UINT64_MAX);
  parse_budget_free(b);

}

TEST(ParseBudget, LimitSaturatesOnTokenProduct) {

  // The per-token limit fits in 64 bits. Its product with the tokens does not.
  parse_budget_t *b = fill(32, 1000000000);
  EXPECT_EQ(parse_budget_limit_ns(b, SIZE_MAX), UINT64_MAX);
  parse_budget_free(b);

}

TEST(ParseBudget, ClockMovesForward) {

  uint64_t a = parse_budget_now_ns();
  uint64_t c = parse_budget_now_ns();
  EXPECT_LE(a, c);

}

TEST(ParseBudget, FreeNullIsSafe) {

  parse_budget_free(NULL);

}
