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

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "custom_mutator.h"
#include "chunk_store_internal.h"
#include "node_stack.h"
#include "tree.h"
#include "tree_mutation.h"
#include "utils.h"
#include "version.h"

// The time of a monotonic clock in microseconds
static double now_us(void) {

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;

}

// The grammar name: the text after "benchmark-" in the base name of argv0
static const char *grammar_name(const char *argv0) {

  const char *base = strrchr(argv0, '/');
  base = base ? base + 1 : argv0;
  static const char prefix[] = "benchmark-";
  if (strncmp(base, prefix, sizeof(prefix) - 1) != 0) return "unknown";
  base += sizeof(prefix) - 1;
  return *base ? base : "unknown";

}

// The signal that asked the program to stop, or 0. The handlers only set this
// flag. The loops check it between library calls and then clean up.
static volatile sig_atomic_t stop_signal = 0;

static void on_signal(int sig) {

  if (!stop_signal) stop_signal = sig;

}

// Handle the signals that stop the program. A second SIGINT, SIGTERM, or
// SIGHUP stops the program at once (SA_RESETHAND), without the cleanup. A
// closed pipe (SIGPIPE) keeps its handler, so that the next write fails with
// an error and the program can clean up.
// For SIGINT and SIGPIPE, sigaction replaces an inherited SIG_IGN on purpose:
// a background job of a shell starts with SIGINT ignored, and the program must
// still clean up. Do not change this to the "keep SIG_IGN" pattern.
// SIGHUP is different. If it is ignored (nohup), the program keeps it ignored,
// so that the run goes on after a hang-up.
static void install_signal_handlers(void) {

  static const int once[] = {SIGINT, SIGTERM, SIGHUP};
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = on_signal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_RESETHAND;
  for (size_t i = 0; i < sizeof(once) / sizeof(once[0]); ++i) {

    struct sigaction old;
    if (once[i] == SIGHUP && sigaction(SIGHUP, NULL, &old) == 0 &&
        old.sa_handler == SIG_IGN)
      continue;
    sigaction(once[i], &action, NULL);

  }

  action.sa_flags = 0;
  sigaction(SIGPIPE, &action, NULL);

}

static bool stopped(void) {

  return stop_signal != 0;

}

static void no_memory(void) {

  fprintf(stderr, "Out of memory\n");

}

// Print a value so that it has no white space
static void print_value(FILE *out, const char *s) {

  for (; *s; ++s) {

    bool blank = *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n';
    fputc(blank ? '?' : *s, out);

  }

}

// The number of all nodes in a tree. It walks with a stack, so a deep tree is
// safe. Return false if the stack cannot grow.
static bool count_nodes(node_t *root, size_t *out) {

  node_stack_t stack = {0};
  size_t       count = 0;
  bool         ok = node_stack_push(&stack, root, 0, 0);
  while (ok && stack.len > 0) {

    node_frame_t frame = node_stack_pop(&stack);
    ++count;
    ok = node_stack_push_children(&stack, frame.node, 0);

  }

  node_stack_free(&stack);
  if (ok) *out = count;
  return ok;

}

// Read a whole number: only digits, not empty, no overflow, at most max
static bool parse_count(const char *s, uint64_t max, uint64_t *out) {

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

    size_t len = 0;
    errno = 0;
    uint8_t *buf = read_file(argv[i], &len);
    int      read_errno = errno;  // printf can change errno
    printf("parse file=");
    print_value(stdout, argv[i]);
    if (!buf) {

      fprintf(stderr, "%s: %s\n", argv[i],
              strerror(read_errno ? read_errno : ENOMEM));
      printf(" bytes=0 nodes=0 us=0.0 result=error\n");
      status = 1;
      continue;

    }

    double  begin = now_us();
    tree_t *tree = tree_from_buf(buf, len);
    double  elapsed = now_us() - begin;
    size_t  nodes = 0;
    bool    counted = !tree || count_nodes(tree->root, &nodes);
    printf(" bytes=%zu nodes=%zu us=%.1f result=%s\n", len, nodes, elapsed,
           !counted ? "error"
           : tree   ? "ok"
                    : "fail");
    if (!counted) status = 1;
    if (tree) tree_free(tree);
    free(buf);

  }

  return status;

}

typedef struct {

  uint64_t seed, entries, steps, trims;

} options_t;

// Read the options of a mode. Return false on a usage error. The one operand
// is the seed folder. Only the run mode takes -e.
static bool parse_options(int argc, char **argv, bool with_entries,
                          options_t *opt, const char **dir) {

  opt->seed = 0;
  opt->entries = 5;
  opt->steps = 1000;
  opt->trims = 1000;
  *dir = NULL;
  int i = 0;
  while (i < argc) {

    if (strcmp(argv[i], "--") == 0) {

      ++i;
      break;

    }

    if (argv[i][0] != '-' || argv[i][1] == '\0') break;
    char flag = argv[i][1];
    // The value is attached to the flag (-n200) or the next argument
    const char *text = argv[i] + 2;
    ++i;
    if (*text == '\0') {

      if (i >= argc) return false;
      text = argv[i++];

    }

    uint64_t *target = NULL;
    uint64_t  min = 0;
    if (flag == 's')
      target = &opt->seed;
    else if (flag == 'e' && with_entries)
      target = &opt->entries, min = 1;
    else if (flag == 'n')
      target = &opt->steps;
    else if (flag == 't')
      target = &opt->trims;
    else
      return false;
    uint64_t value;
    if (!parse_count(text, UINT32_MAX, &value) || value < min) return false;
    *target = value;

  }

  if (argc - i != 1) return false;
  *dir = argv[i];
  return true;

}

typedef struct {

  char **names;
  size_t count;

} seed_list_t;

static void seed_list_free(seed_list_t *list) {

  for (size_t i = 0; i < list->count; ++i)
    free(list->names[i]);
  free(list->names);
  list->names = NULL;
  list->count = 0;

}

static int compare_names(const void *a, const void *b) {

  return strcmp(*(char *const *)a, *(char *const *)b);

}

// Join a folder and a name. Return NULL (with a message) if there is no memory.
static char *join_path(const char *dir, const char *name) {

  size_t len = strlen(dir) + strlen(name) + 2;
  char  *path = malloc(len);
  if (!path) {

    no_memory();
    return NULL;

  }

  snprintf(path, len, "%s/%s", dir, name);
  return path;

}

// List the regular files at the top level of a folder, without dot files, in
// byte order of the names. Return false on an error.
static bool seed_list_load(const char *dir, seed_list_t *list) {

  list->names = NULL;
  list->count = 0;
  DIR *dp = opendir(dir);
  if (!dp) return false;
  size_t         cap = 0;
  bool           ok = true;
  struct dirent *entry;
  while (ok && (entry = readdir(dp)) != NULL) {

    if (entry->d_name[0] == '.') continue;
    char *path = join_path(dir, entry->d_name);
    if (!path) {

      ok = false;
      break;

    }

    struct stat st;
    bool        regular = stat(path, &st) == 0 && S_ISREG(st.st_mode);
    free(path);
    if (!regular) continue;
    if (list->count == cap) {

      cap = cap ? cap * 2 : 64;
      char **bigger = realloc(list->names, cap * sizeof(char *));
      if (!bigger) {

        no_memory();
        ok = false;
        break;

      }

      list->names = bigger;

    }

    char *name = strdup(entry->d_name);
    if (!name) {

      no_memory();
      ok = false;
      break;

    }

    list->names[list->count++] = name;

  }

  closedir(dp);
  if (!ok) {

    seed_list_free(list);
    return false;

  }

  if (list->count > 1)
    qsort(list->names, list->count, sizeof(char *), compare_names);
  return true;

}

// Write a whole file. Return false on an error.
static bool write_file(const char *path, const uint8_t *buf, size_t len) {

  FILE *fp = fopen(path, "wb");
  if (!fp) return false;
  bool ok = fwrite(buf, 1, len, fp) == len;
  if (fclose(fp) != 0) ok = false;
  return ok;

}

// Make the work folder and copy the seeds to its queue folder. Return the path
// of the work folder, or NULL on an error. The caller removes the folder.
static char *work_create(const char *seed_dir, const seed_list_t *seeds) {

  const char *tmp = getenv("TMPDIR");
  if (!tmp || !*tmp) tmp = "/tmp";
  size_t len = strlen(tmp) + sizeof("/benchmark.XXXXXX");
  char  *work = malloc(len);
  if (!work) {

    no_memory();
    return NULL;

  }

  snprintf(work, len, "%s/benchmark.XXXXXX", tmp);
  if (!mkdtemp(work)) {

    perror("Cannot make the work folder");
    free(work);
    return NULL;

  }

  // From here, a signal ends the run through the cleanup of the work folder
  install_signal_handlers();

  char *queue = join_path(work, "queue");
  bool  ok = queue && create_directory(queue);
  for (size_t i = 0; ok && i < seeds->count; ++i) {

    if (stopped()) {

      ok = false;
      break;

    }

    char    *from = join_path(seed_dir, seeds->names[i]);
    char    *to = join_path(queue, seeds->names[i]);
    size_t   size = 0;
    uint8_t *buf = from ? read_file(from, &size) : NULL;
    ok = buf && to && write_file(to, buf, size);
    if (!ok) fprintf(stderr, "Cannot copy the seed %s\n", seeds->names[i]);
    free(buf);
    free(from);
    free(to);

  }

  free(queue);
  if (!ok) {

    remove_directory(work);
    free(work);
    return NULL;

  }

  return work;

}

// Set the step counts of the mutator, so that the results do not depend on the
// shell. The parse time limit is off unless the user sets it.
static void set_step_variables(uint64_t steps, uint64_t trims) {

  static const char *const names[] = {

      "RULES_MUTATION_STEPS", "RANDOM_MUTATION_STEPS",
      "RANDOM_RECURSIVE_MUTATION_STEPS", "SPLICING_MUTATION_STEPS"};
  char value[32];
  snprintf(value, sizeof(value), "%llu", (unsigned long long)steps);
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    setenv(names[i], value, 1);
  snprintf(value, sizeof(value), "%llu", (unsigned long long)trims);
  setenv("TRIMMING_STEPS", value, 1);
  const char *budget = getenv("PARSE_TIME_BUDGET_MS");
  if (!budget || !*budget) setenv("PARSE_TIME_BUDGET_MS", "0", 1);

}

static void print_header(FILE *out, const char *grammar, const char *mode,
                         const options_t *opt, size_t seeds, size_t trees) {

  struct utsname un;
  if (uname(&un) != 0) {

    snprintf(un.sysname, sizeof(un.sysname), "unknown");
    snprintf(un.machine, sizeof(un.machine), "unknown");

  }

  int optimized = 0;
#ifdef __OPTIMIZE__
  optimized = 1;
#endif
  fprintf(out, "benchmark version=%s grammar=", GRAMMAR_MUTATOR_VERSION);
  print_value(out, grammar);
  fprintf(out, " system=");
  print_value(out, un.sysname);
  fprintf(out, " machine=");
  print_value(out, un.machine);
  fprintf(out, " optimized=%d mode=%s seed=%llu", optimized, mode,
          (unsigned long long)opt->seed);
  if (strcmp(mode, "run") == 0)
    fprintf(out, " entries=%llu", (unsigned long long)opt->entries);
  const char *budget = getenv("PARSE_TIME_BUDGET_MS");
  fprintf(out, " steps=%llu trims=%llu parse_budget_ms=",
          (unsigned long long)opt->steps, (unsigned long long)opt->trims);
  print_value(out, budget ? budget : "");
  fprintf(out, " seeds=%zu trees=%zu\n", seeds, trees);

}

// The peak resident memory in KB
static long peak_rss_kb(void) {

  struct rusage ru;
  if (getrusage(RUSAGE_SELF, &ru) != 0) return 0;
#ifdef __APPLE__
  return ru.ru_maxrss / 1024;  // bytes on macOS
#else
  return ru.ru_maxrss;
#endif

}

// The mutator keeps the pointer of the last path in filename_cur. The path
// stays alive until the next queue_get call or until bench_close.
static char *kept_path;

// Call afl_custom_queue_get with a copy of the path. Return 0 on an error.
static uint8_t queue_get(my_mutator_t *data, const char *path) {

  char *copy = strdup(path);
  if (!copy) {

    no_memory();
    return 0;

  }

  uint8_t result = afl_custom_queue_get(data, (const uint8_t *)copy);
  free(kept_path);
  kept_path = copy;
  return result;

}

// Say if a trim step is a success. It gets the output of the trim step.
typedef bool (*trim_rule_t)(void *ctx, size_t step, const uint8_t *out,
                            size_t len);

// Run all trim steps of the current entry, as AFL++ does. A NULL rule means
// that every step fails. The rule call is not in the timed part. Return false
// if a trim step gives no output.
static bool trim_entry(my_mutator_t *data, trim_rule_t rule, void *ctx,
                       size_t *steps, double *elapsed_us) {

  *steps = 0;
  *elapsed_us = 0;
  int32_t max = afl_custom_init_trim(data, NULL, 0);
  int32_t cur = 0;
  while (cur < max) {

    if (stopped()) return false;
    uint8_t *out = NULL;
    double   begin = now_us();
    size_t   len = afl_custom_trim(data, &out);
    *elapsed_us += now_us() - begin;
    if (!out) {

      fprintf(stderr, "A trim step gave no output\n");
      return false;

    }

    int success = rule ? rule(ctx, *steps, out, len) : 0;
    begin = now_us();
    cur = afl_custom_post_trim(data, success);
    *elapsed_us += now_us() - begin;
    ++*steps;

  }

  return true;

}

typedef struct {

  double us;
  size_t bytes;
  bool   tree;

} seed_info_t;

static const char *const stage_names[] = {"rules", "random", "recursive",
                                          "splice"};

// Run the planned fuzz calls of the current entry and print one stage record
// for each stage with planned steps. Every output counts toward the stage that
// the mutator has before the call. For an output that is not NULL, this is the
// stage of afl_custom_describe. A NULL output has no other stage to count
// toward. After the last stage (the rules stage ended early), the output counts
// toward the last stage. Return false on an error.
static bool measure_stages(my_mutator_t *data, const char *name,
                           uint32_t planned, const size_t steps[4],
                           uint64_t same) {

  hash_set_t *sets[4] = {NULL};
  bool        ok = true;
  for (int i = 0; i < 4; ++i) {

    sets[i] = hash_set_create();
    if (!sets[i]) {

      no_memory();
      ok = false;

    }

  }

  size_t outputs[4] = {0}, total_bytes[4] = {0}, dups[4] = {0}, sames[4] = {0};
  double us[4] = {0};
  for (uint32_t k = 0; ok && k < planned; ++k) {

    if (stopped()) {

      ok = false;
      break;

    }

    uint8_t stage = data->cur_fuzzing_stage;
    if (stage > 3) stage = 3;
    uint8_t *out = NULL;
    double   begin = now_us();
    size_t   out_len = afl_custom_fuzz(data, NULL, 0, &out, NULL, 0, 1 << 20);
    double   elapsed = now_us() - begin;
    if (!out) out_len = 0;
    uint64_t hash = hash_bytes(out ? (const void *)out : "", out_len);
    ++outputs[stage];
    us[stage] += elapsed;
    total_bytes[stage] += out_len;
    if (!hash_set_add(sets[stage], hash)) ++dups[stage];
    if (hash == same) ++sames[stage];

  }

  for (int i = 0; ok && i < 4; ++i) {

    if (steps[i] == 0) continue;
    size_t n = outputs[i] > 0 ? outputs[i] : 1;
    printf("stage entry=");
    print_value(stdout, name);
    printf(
        " stage=%s steps=%zu us_per_output=%.1f avg_bytes=%zu "
        "dup_pct=%.1f same_pct=%.1f\n",
        stage_names[i], steps[i], us[i] / (double)n, total_bytes[i] / n,
        (double)dups[i] * 100.0 / (double)n,
        (double)sames[i] * 100.0 / (double)n);

  }

  for (int i = 0; i < 4; ++i)
    if (sets[i]) hash_set_free(sets[i]);
  return ok;

}

// Measure one entry: print the entry, stage, and trim records
static bool measure_entry(my_mutator_t *data, const char *queue,
                          const char *name) {

  char *path = join_path(queue, name);
  if (!path) return false;
  size_t   len = 0;
  uint8_t *bytes = read_file(path, &len);
  if (!bytes) {

    fprintf(stderr, "Cannot read %s\n", path);
    free(path);
    return false;

  }

  uint64_t same = hash_bytes(bytes, len);
  free(bytes);

  // Pick the entry once untimed. Then the timed pick frees the tree of this
  // entry, not the last seed, so repick_us follows the size of the entry.
  double begin = 0;
  double repick_us = 0;
  if (!queue_get(data, path)) {

    fprintf(stderr, "Cannot get the entry %s\n", name);
    free(path);
    return false;

  }

  begin = now_us();
  uint8_t picked = queue_get(data, path);
  repick_us = now_us() - begin;
  free(path);
  if (!picked) {

    fprintf(stderr, "Cannot get the entry %s\n", name);
    return false;

  }

  size_t nodes = 0;
  if (!data->tree_cur) {

    fprintf(stderr, "No tree for %s\n", name);
    return false;

  }

  if (!count_nodes(data->tree_cur->root, &nodes)) {

    fprintf(stderr, "Cannot count the nodes of %s\n", name);
    return false;

  }

  size_t rules_pairs = rules_mutation_count(data->tree_cur);
  begin = now_us();
  uint32_t planned = afl_custom_fuzz_count(data, NULL, 0);
  double   count_us = now_us() - begin;
  // With no planned output, the counts of the previous entry are stale
  size_t steps[4] = {0};
  if (planned > 0) {

    steps[0] = data->total_rules_mutation_steps;
    steps[1] = data->total_random_mutation_steps;
    steps[2] = data->total_random_recursive_mutation_steps;
    steps[3] = data->total_splicing_mutation_steps;

  }

  printf("entry name=");
  print_value(stdout, name);
  printf(" bytes=%zu nodes=%zu rules_pairs=%zu repick_us=%.1f count_us=%.1f\n",
         len, nodes, rules_pairs, repick_us, count_us);

  // Flush each part, so that a reader sees the progress and a closed pipe
  // shows early
  fflush(stdout);
  if (!measure_stages(data, name, planned, steps, same)) return false;
  fflush(stdout);

  size_t trim_steps;
  double trim_us;
  if (!trim_entry(data, NULL, NULL, &trim_steps, &trim_us)) return false;
  printf("trim entry=");
  print_value(stdout, name);
  printf(" steps=%zu us_per_step=%.1f\n", trim_steps,
         trim_steps ? trim_us / (double)trim_steps : 0.0);
  return true;

}

// The seed names and sizes that compare_by_size reads. The qsort call has no
// argument for them. The entries are ranked by (bytes, name).
static const seed_list_t *sort_seeds;
static const seed_info_t *sort_infos;

static int compare_by_size(const void *a, const void *b) {

  size_t x = *(const size_t *)a, y = *(const size_t *)b;
  if (sort_infos[x].bytes != sort_infos[y].bytes)
    return sort_infos[x].bytes < sort_infos[y].bytes ? -1 : 1;
  return strcmp(sort_seeds->names[x], sort_seeds->names[y]);

}

// The setup that the run and digest modes share: the options, the seeds, the
// work folder with the queue folder, and the mutator. Start with bench_ctx_t
// ctx = {0}. Always call bench_close, also after a failure of bench_open.
typedef struct {

  options_t     opt;
  const char   *seed_dir;
  seed_list_t   seeds;
  char         *work;
  char         *queue;
  my_mutator_t *data;

} bench_ctx_t;

// Open the setup. Return 0 on success, else the exit status of the program.
static int bench_open(bench_ctx_t *ctx, const char *prog, bool with_entries,
                      int argc, char **argv) {

  if (!parse_options(argc, argv, with_entries, &ctx->opt, &ctx->seed_dir))
    return usage(prog);
  if (!seed_list_load(ctx->seed_dir, &ctx->seeds)) {

    fprintf(stderr, "Cannot read the seed folder %s\n", ctx->seed_dir);
    return 1;

  }

  if (ctx->seeds.count == 0) {

    fprintf(stderr, "No seed file in %s\n", ctx->seed_dir);
    return 1;

  }

  ctx->work = work_create(ctx->seed_dir, &ctx->seeds);
  if (!ctx->work) return 1;
  ctx->queue = join_path(ctx->work, "queue");
  if (!ctx->queue) return 1;
  set_step_variables(ctx->opt.steps, ctx->opt.trims);
  ctx->data = afl_custom_init(NULL, (unsigned)ctx->opt.seed);
  if (!ctx->data) fprintf(stderr, "Cannot start the mutator\n");
  return ctx->data ? 0 : 1;

}

// Close the setup: stop the mutator and remove the work folder
static void bench_close(bench_ctx_t *ctx) {

  if (ctx->data) afl_custom_deinit(ctx->data);
  free(kept_path);
  kept_path = NULL;
  free(ctx->queue);
  if (ctx->work) remove_directory(ctx->work);
  free(ctx->work);
  seed_list_free(&ctx->seeds);

}

// The run mode: measure the mutator on the seeds of a folder
static int mode_run(const char *prog, int argc, char **argv) {

  bench_ctx_t ctx = {0};
  int         status = bench_open(&ctx, prog, true, argc, argv);
  if (status != 0) {

    bench_close(&ctx);
    return status;

  }

  const options_t   opt = ctx.opt;
  const seed_list_t seeds = ctx.seeds;
  const char       *seed_dir = ctx.seed_dir;
  const char       *queue = ctx.queue;
  my_mutator_t     *data = ctx.data;
  status = 1;
  seed_info_t *infos = calloc(seeds.count, sizeof(seed_info_t));
  size_t      *order = calloc(seeds.count, sizeof(size_t));
  if (!infos || !order) {

    no_memory();
    goto done;

  }

  size_t trees = 0;
  double fill_us = 0;
  for (size_t i = 0; i < seeds.count; ++i) {

    if (stopped()) goto done;
    char       *path = join_path(queue, seeds.names[i]);
    struct stat st;
    if (!path) goto done;
    infos[i].bytes = stat(path, &st) == 0 ? (size_t)st.st_size : 0;
    double  begin = now_us();
    uint8_t got = queue_get(data, path);
    infos[i].us = now_us() - begin;
    if (!got) {

      fprintf(stderr, "Cannot get the entry %s\n", path);
      free(path);
      goto done;

    }

    infos[i].tree = data->tree_cur != NULL;
    fill_us += infos[i].us;
    if (infos[i].tree) order[trees++] = i;
    free(path);

  }

  if (trees == 0) {

    fprintf(stderr, "No seed in %s gives a tree\n", seed_dir);
    goto done;

  }

  print_header(stdout, grammar_name(prog), "run", &opt, seeds.count, trees);
  for (size_t i = 0; i < seeds.count; ++i) {

    printf("seed name=");
    print_value(stdout, seeds.names[i]);
    printf(" bytes=%zu parse_us=%.1f tree=%d\n", infos[i].bytes, infos[i].us,
           infos[i].tree ? 1 : 0);

  }

  printf("store seeds=%zu chunks=%zu fill_ms=%.1f\n", trees,
         (size_t)seen_chunks.base.nnodes, fill_us / 1e3);

  sort_seeds = &seeds;
  sort_infos = infos;
  qsort(order, trees, sizeof(size_t), compare_by_size);
  size_t wanted = opt.entries < trees ? (size_t)opt.entries : trees;
  size_t last_rank = SIZE_MAX;
  status = 0;
  for (size_t i = 0; i < wanted && status == 0; ++i) {

    if (stopped()) {

      status = 1;
      break;

    }

    size_t rank =
        wanted == 1 ? trees - 1
                    : (2 * i * (trees - 1) + (wanted - 1)) / (2 * (wanted - 1));
    if (rank == last_rank) continue;
    last_rank = rank;
    if (!measure_entry(data, queue, seeds.names[order[rank]])) status = 1;

  }

  if (status == 0) printf("peak rss_kb=%ld\n", peak_rss_kb());
  fflush(stdout);

done:
  free(infos);
  free(order);
  bench_close(&ctx);
  return status;

}

// The 64-bit FNV-1a hash of the outputs. Each buffer adds its bytes and then
// its length as 8 little-endian bytes.
#define FNV_OFFSET 0xcbf29ce484222325ULL
#define FNV_PRIME 0x100000001b3ULL

static void mix_byte(uint64_t *h, uint8_t byte) {

  *h = (*h ^ byte) * FNV_PRIME;

}

static void mix(uint64_t *h, const uint8_t *buf, size_t len) {

  for (size_t i = 0; i < len; ++i)
    mix_byte(h, buf[i]);
  for (int i = 0; i < 8; ++i)
    mix_byte(h, (uint8_t)((uint64_t)len >> (8 * i)));

}

static void mix_text(uint64_t *h, const char *text) {

  mix(h, (const uint8_t *)text, strlen(text));

}

typedef struct {

  uint64_t hash;
  size_t   outputs, new_entries, trims;

} digest_t;

// The trim rule of the digest mode: add the output to the hash. Every 8th
// step is a success, if the output is not empty.
static bool digest_trim_rule(void *ctx, size_t step, const uint8_t *out,
                             size_t len) {

  digest_t *digest = ctx;
  mix(&digest->hash, out, len);
  ++digest->trims;
  return (step + 1) % 8 == 0 && len > 0;

}

// Process one queue file of the digest mode: all fuzz outputs, a new entry
// from the last output (for a seed only), and all trim steps. Return false on
// an error.
static bool digest_entry(my_mutator_t *data, const char *work,
                         const char *entry_file, bool is_seed,
                         digest_t *digest) {

  if (!queue_get(data, entry_file)) {

    fprintf(stderr, "Cannot get the entry %s\n", entry_file);
    return false;

  }

  uint32_t planned = afl_custom_fuzz_count(data, NULL, 0);
  uint8_t *last = NULL;
  size_t   last_len = 0;
  bool     ok = true;
  for (uint32_t k = 0; k < planned; ++k) {

    if (stopped()) {

      ok = false;
      break;

    }

    uint8_t *out = NULL;
    size_t   len = afl_custom_fuzz(data, NULL, 0, &out, NULL, 0, 1 << 20);
    ++digest->outputs;
    if (!out) {

      mix(&digest->hash, (const uint8_t *)"NULL", 4);
      continue;

    }

    mix(&digest->hash, out, len);
    if (len > 0) {

      uint8_t *copy = realloc(last, len);
      if (!copy) {

        no_memory();
        ok = false;
        break;

      }

      memcpy(copy, out, len);
      last = copy;
      last_len = len;

    }

  }

  if (ok && last && is_seed) {

    char name[32];
    snprintf(name, sizeof(name), "queue/.new_%06zu", digest->new_entries);
    char *new_file = join_path(work, name);
    ok = new_file && write_file(new_file, last, last_len);
    if (ok) {

      // The return value says that the file changed (AFL++ API), not an
      // error, so it is not checked. The last output can be NULL or empty,
      // so mutated_tree may not be the tree of these bytes. Then the mutator
      // compares the bytes and parses the file.
      afl_custom_queue_new_entry(data, (const uint8_t *)new_file,
                                 (const uint8_t *)entry_file);
      ++digest->new_entries;

    } else if (new_file) {

      fprintf(stderr, "Cannot write the new entry\n");

    }

    free(new_file);

  }

  free(last);
  if (!ok) return false;

  double elapsed;
  size_t steps;
  return trim_entry(data, digest_trim_rule, digest, &steps, &elapsed);

}

// The digest mode: hash all outputs of the mutator, to check a change
static int mode_digest(const char *prog, int argc, char **argv) {

  bench_ctx_t ctx = {0};
  int         status = bench_open(&ctx, prog, false, argc, argv);
  if (status != 0) {

    bench_close(&ctx);
    return status;

  }

  const seed_list_t seeds = ctx.seeds;
  const char       *seed_dir = ctx.seed_dir;
  const char       *queue = ctx.queue;
  const char       *work = ctx.work;
  my_mutator_t     *data = ctx.data;
  status = 1;

  digest_t digest = {FNV_OFFSET, 0, 0, 0};
  size_t   trees = 0;
  for (size_t i = 0; i < seeds.count; ++i) {

    if (stopped()) goto done;
    char *path = join_path(queue, seeds.names[i]);
    if (!path) goto done;
    if (!queue_get(data, path)) {

      fprintf(stderr, "Cannot get the entry %s\n", path);
      free(path);
      goto done;

    }

    if (!data->tree_cur) {

      mix_text(&digest.hash, seeds.names[i]);
      mix_text(&digest.hash, "NOTREE");
      free(path);
      continue;

    }

    ++trees;
    bool ok = digest_entry(data, work, path, true, &digest);
    free(path);
    if (!ok) goto done;

  }

  if (trees == 0) {

    fprintf(stderr, "No seed in %s gives a tree\n", seed_dir);
    goto done;

  }

  size_t seed_entries = digest.new_entries;
  for (size_t i = 0; i < seed_entries; ++i) {

    char name[32];
    snprintf(name, sizeof(name), "queue/.new_%06zu", i);
    char *path = join_path(work, name);
    if (!path) goto done;
    bool ok = digest_entry(data, work, path, false, &digest);
    free(path);
    if (!ok) goto done;

  }

  print_header(stdout, grammar_name(prog), "digest", &ctx.opt, seeds.count,
               trees);
  printf("digest outputs=%zu new_entries=%zu trims=%zu hash=%016llx\n",
         digest.outputs, digest.new_entries, digest.trims,
         (unsigned long long)digest.hash);
  fflush(stdout);
  status = 0;

done:
  bench_close(&ctx);
  return status;

}

static int run_mode(int argc, char **argv);

int main(int argc, char **argv) {

  int status = run_mode(argc, argv);
  if (stopped()) {

    // The mode did its cleanup. Stop with the signal, so that the shell sees
    // it.
    fflush(stdout);
    fprintf(stderr, "Stopped by signal %d\n", (int)stop_signal);
    signal((int)stop_signal, SIG_DFL);
    raise((int)stop_signal);

  }

  // A write error other than a closed pipe (for example ENOSPC or EBADF)
  if ((fflush(stdout) != 0 || ferror(stdout)) && status == 0) {

    fprintf(stderr, "Cannot write the output\n");
    status = 1;

  }

  return status;

}

static int run_mode(int argc, char **argv) {

  if (argc < 2) return usage(argv[0]);
  const char *mode = argv[1];
  if (strcmp(mode, "parse") == 0) {

    if (argc < 3) return usage(argv[0]);
    int status = mode_parse(argc - 2, argv + 2);
    fflush(stdout);
    return status;

  }

  if (strcmp(mode, "run") == 0) return mode_run(argv[0], argc - 2, argv + 2);
  if (strcmp(mode, "digest") == 0)
    return mode_digest(argv[0], argc - 2, argv + 2);
  return usage(argv[0]);

}
