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

#ifndef __MUTATOR_BENCHMARK_H__
#define __MUTATOR_BENCHMARK_H__

#ifdef __cplusplus
extern "C" {
#endif

void bench_all();
void bench_parsing_test_case(const char *fn);

void bench_generating();
void bench_parsing();
void bench_mutation();
void bench_random_mutation();
void bench_random_recursive_mutation();
void bench_splicing_mutation();
void bench_trimming();
void bench_subtree_trimming();
void bench_recursive_trimming();

void bench_stats_print(const char *label);

#ifdef __cplusplus
}
#endif

#endif
