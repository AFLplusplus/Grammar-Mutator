#ifndef __MUTATOR_BENCHMARK_H__
#define __MUTATOR_BENCHMARK_H__

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

#endif
