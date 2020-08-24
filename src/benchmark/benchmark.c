#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include "benchmark.h"
#include "f1_c_fuzz.h"
#include "tree.h"
#include "tree_mutation.h"
#include "tree_trimming.h"

#define BENCH_NUM (1000)
#define MAX_TREE_LEN (500 + 1)
#define MAX_LABEL_LEN (100)

static double current_time() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
}

static double start, end;  // start and end time
static double times[BENCH_NUM];
static char   label[MAX_LABEL_LEN];

void bench_generating() {
  tree_t *tree;

  printf("========== Generating [START] ==========\n");
  for (int max_len = 0; max_len < MAX_TREE_LEN; max_len += 50) {
    for (int i = 0; i < BENCH_NUM; ++i) {
      start = current_time();
      tree = gen_init__(max_len);
      end = current_time();
      times[i] = (end - start);

      tree_free(tree);
    }
    snprintf(label, MAX_LABEL_LEN, "Generating, max_len=%d", max_len);
    bench_stats_print(label);
  }
  printf("=========== Generating [END] ===========\n\n");
}

void bench_parsing() {
  tree_t *tree, *recovered_tree;

  printf("========== Parsing [START] ==========\n");
  for (int max_len = 0; max_len < MAX_TREE_LEN; max_len += 50) {
    for (int i = 0; i < BENCH_NUM; ++i) {
      tree = gen_init__(max_len);
      tree_to_buf(tree);

      start = current_time();
      recovered_tree = tree_from_buf(tree->data_buf, tree->data_len);
      end = current_time();
      times[i] = (end - start);

      tree_free(recovered_tree);
      tree_free(tree);
    }
    snprintf(label, MAX_LABEL_LEN, "Parsing, max_len=%d", max_len);
    bench_stats_print(label);
  }
  printf("=========== Parsing [END] ===========\n\n");
}

inline void bench_mutation() {
  //  bench_random_mutation();
  bench_random_recursive_mutation();
  bench_splicing_mutation();
}

void bench_random_mutation() {
  tree_t *tree, *mutated_tree;

  printf("========== Random Mutation [START] ==========\n");
  for (int max_len = 0; max_len < MAX_TREE_LEN; max_len += 50) {
    for (int i = 0; i < BENCH_NUM; ++i) {
      tree = gen_init__(max_len);
      tree_get_size(tree);

      start = current_time();
      mutated_tree = random_mutation(tree);
      end = current_time();
      times[i] = (end - start);

      tree_free(mutated_tree);
      tree_free(tree);
    }
    snprintf(label, MAX_LABEL_LEN, "Random mutation, max_len=%d", max_len);
    bench_stats_print(label);
  }
  printf("=========== Random Mutation [END] ===========\n\n");
}

void bench_random_recursive_mutation() {
  tree_t *tree, *mutated_tree;
  int     n;

  printf("========== Random Recursive Mutation [START] ==========\n");
  for (int max_len = 0; max_len < MAX_TREE_LEN; max_len += 50) {
    for (int i = 0; i < BENCH_NUM; ++i) {
      tree = gen_init__(max_len);
      tree_get_size(tree);
      n = random() % 16;

      start = current_time();
      mutated_tree = random_recursive_mutation(tree, n);
      end = current_time();
      times[i] = (end - start);

      tree_free(mutated_tree);
      tree_free(tree);
    }
    snprintf(label, MAX_LABEL_LEN, "Random recursive mutation, max_len=%d",
             max_len);
    bench_stats_print(label);
  }
  printf("=========== Random Recursive Mutation [END] ===========\n\n");
}

void bench_splicing_mutation() {
  // TODO: this relies on the chunk store
}

inline void bench_trimming() {
  bench_subtree_trimming();
  bench_recursive_trimming();
}

void bench_subtree_trimming() {
  tree_t *tree, *trimmed_tree;
  node_t *node;
  int max_len;

  printf("========== Subtree Trimming, Single Node [START] ==========\n");
  for (int i = 0; i < BENCH_NUM; ++i) {
    max_len = random() % MAX_TREE_LEN;
    tree = gen_init__(max_len);
    tree_get_non_terminal_nodes(tree);
    int j = random() % tree->non_terminal_node_list->size;
    node = list_get(tree->non_terminal_node_list, j);

    start = current_time();
    trimmed_tree = subtree_trimming(tree, node);
    end = current_time();
    times[i] = (end - start);

    tree_free(trimmed_tree);
    tree_free(tree);
  }
  snprintf(label, MAX_LABEL_LEN, "Subtree trimming, single node");
  bench_stats_print(label);
  printf("=========== Subtree Trimming, Single Node [END] ===========\n\n");
}

void bench_recursive_trimming() {
  tree_t *tree, *trimmed_tree;
  edge_t *edge;
  int max_len;

  printf("========== Recursive Trimming, Single Node [START] ==========\n");
  tree = gen_init__(MAX_TREE_LEN);
  tree_get_recursion_edges(tree);
  while (tree->recursion_edge_list->size == 0) {
    tree_free(tree);
    tree = gen_init__(MAX_TREE_LEN);
    tree_get_recursion_edges(tree);
  }
  for (int i = 0, j = 0; i < BENCH_NUM; ++i) {
    j = random() % tree->recursion_edge_list->size;
    edge = list_get(tree->recursion_edge_list, j);

    start = current_time();
    trimmed_tree = recursive_trimming(tree, *edge);
    end = current_time();
    times[i] = (end - start);

    tree_free(trimmed_tree);
  }
  tree_free(tree);
  snprintf(label, MAX_LABEL_LEN, "Recursive trimming, single node");
  bench_stats_print(label);
  printf("=========== Recursive Trimming, Single Node [END] ===========\n\n");
}

/**
 * The algorithm used to calculate the average and standard deviation can avoid
 * overflow.
 *
 * Reference:
 * https://stackoverflow.com/questions/1930454/what-is-a-good-solution-for-calculating-an-average-where-the-sum-of-all-values-e
 */
void bench_stats_print(const char *label) {
  double time_avg = 0, time_var = 0, time_std = 0;
  // Avoid overflow
  for (int i = 0; i < BENCH_NUM; ++i) {
    time_avg += (times[i] - time_avg) / (i + 1);
  }
  for (int i = 0; i < BENCH_NUM; ++i) {
    time_var += (pow(times[i] - time_avg, 2) - time_var) / (i + 1);
  }
  time_std = sqrt(time_var);

  printf("%s - avg: %lf s, std: %lf\n", label, time_avg, time_std);
#ifdef DEBUG_BUILD
  if (time_std < time_avg) return;

  printf("[DEBUG] Raw data: ");
  for (int i = 0; i < BENCH_NUM; ++i) {
    time_avg += times[i];
    printf("%lf ", times[i]);
  }
  printf("\n");
#endif
}

int main(int argc, const char *argv[]) {
  srandom(time(NULL));

  bench_generating();
  bench_parsing();
  bench_mutation();
  bench_trimming();
  return 0;
}