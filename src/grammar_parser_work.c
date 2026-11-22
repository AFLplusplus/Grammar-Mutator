/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   grammar_parser-<name>: the worker threads parse the files. The main thread
   writes the trees and prints the messages in file order.
 */

#include <errno.h>
#include <pthread.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "grammar_parser_internal.h"
#include "tree.h"

static const char *reasons[REASON_COUNT] = {

    "not UTF-8", "syntax error", "parser limit", "duplicate name", "no match"};

typedef struct {

  job_t          *jobs;
  size_t          count;
  size_t          next;      // the next job for a worker
  size_t          consumed;  // the number of jobs that the main thread used
  size_t          window;    // the maximum number of jobs before `consumed`
  int             stop;
  pthread_mutex_t lock;
  pthread_cond_t  ready;  // a job is done
  pthread_cond_t  space;  // `consumed` increased, or `stop` is set

} work_t;

// Compare the tree files of two jobs by device and inode
static int compare_tree_files(const void *a, const void *b) {

  const job_t *x = a, *y = b;
  if (x->tree_dev != y->tree_dev) return x->tree_dev < y->tree_dev ? -1 : 1;
  if (x->tree_ino != y->tree_ino) return x->tree_ino < y->tree_ino ? -1 : 1;
  return 0;

}

// Read and parse the file of one job (in a worker thread)
static void parse_job(job_t *job) {

  size_t   len = 0;
  uint8_t *buf = gp_read_file(job->path, &len);
  if (!buf) {

    job->kind = RESULT_READ_ERROR;
    job->error = errno;
    return;

  }

  parse_status_t status = PARSE_NO_MATCH;
  tree_t        *tree = tree_parse(buf, len, NULL, &status);
  free(buf);
  if (status == PARSE_THREAD_ERROR) {

    tree_free(tree);
    job->kind = RESULT_THREAD_ERROR;
    return;

  }

  if (!tree) {

    job->kind = RESULT_SKIP;
    switch (status) {

      case PARSE_NOT_UTF8:
        job->reason = REASON_NOT_UTF8;
        break;
      case PARSE_SYNTAX_ERROR:
        job->reason = REASON_SYNTAX_ERROR;
        break;
      case PARSE_LIMIT:
        job->reason = REASON_PARSER_LIMIT;
        break;
      default:
        job->reason = REASON_NO_MATCH;
        break;

    }

    return;

  }

  // Keep the serialized tree, and free the rest
  if (!tree_serialize(tree)) {

    tree_free(tree);
    job->kind = RESULT_MEMORY_ERROR;
    return;

  }

  job->kind = RESULT_TREE;
  job->tree_buf = tree->ser_buf;
  job->tree_len = tree->ser_len;
  tree->ser_buf = NULL;
  tree->ser_size = 0;
  tree->ser_len = 0;
  tree_free(tree);

}

static void *worker(void *arg) {

  work_t *work = arg;

  pthread_mutex_lock(&work->lock);
  for (;;) {

    while (!work->stop && work->next < work->count &&
           work->next >= work->consumed + work->window)
      pthread_cond_wait(&work->space, &work->lock);
    if (work->stop || work->next >= work->count) break;
    job_t *job = &work->jobs[work->next++];
    pthread_mutex_unlock(&work->lock);

    parse_job(job);

    pthread_mutex_lock(&work->lock);
    job->done = 1;
    pthread_cond_signal(&work->ready);

  }

  pthread_mutex_unlock(&work->lock);
  return NULL;

}

// Use the result of one job: write the tree, or report the skip. `written`
// is the search tree of the jobs with a tree file. Return 0, or -1 with a
// message.
static int use_result(job_t *job, const char *trees_dir, void **written,
                      size_t *parsed, size_t *skipped) {

  // AFL++ changes each space in a queue name to "_" (no_spaces() in
  // afl-fuzz-init.c), and the mutator looks for the tree with that name.
  const char *slash = strrchr(job->relative, '/');
  char       *name = gp_xstrdup(slash ? slash + 1 : job->relative);
  for (char *c = name; *c; ++c)
    if (*c == ' ') *c = '_';
  char *tree_path = gp_join(trees_dir, name);
  free(name);

  // Two tree names are the same name if the file system gives them one file,
  // for example A.json and a.json on macOS. An error wins over a duplicate
  // name, so only a tree or a skip can become a duplicate name.
  struct stat st;
  int         ret = 0;
  if ((job->kind == RESULT_TREE || job->kind == RESULT_SKIP) &&
      !stat(tree_path, &st)) {

    job->tree_dev = st.st_dev;
    job->tree_ino = st.st_ino;
    if (tfind(job, written, compare_tree_files)) {

      job->kind = RESULT_SKIP;
      job->reason = REASON_DUPLICATE_NAME;

    }

  }

  switch (job->kind) {

    case RESULT_READ_ERROR:
      gp_report("[-] Cannot read ", job->relative, "", job->error);
      ret = -1;
      break;

    case RESULT_THREAD_ERROR:
      gp_report("[-] Cannot start the parser thread for ", job->relative, "",
                0);
      ret = -1;
      break;

    case RESULT_MEMORY_ERROR:
      gp_report("[-] Out of memory for the tree of ", job->relative, "", 0);
      ret = -1;
      break;

    case RESULT_SKIP:
      ++skipped[job->reason];
      fputs("[!] ", stderr);
      gp_print_name(stderr, job->relative);
      fprintf(stderr, ": %s\n", reasons[job->reason]);
      if (job->reason != REASON_DUPLICATE_NAME)
        ret = gp_remove_old_tree(tree_path, job->relative);
      break;

    case RESULT_TREE:
      if (gp_write_file(tree_path, job->tree_buf, job->tree_len)) {

        gp_report("[-] Cannot write the tree of ", job->relative, "", errno);
        ret = -1;
        break;

      }

      if (!stat(tree_path, &st)) {

        job->tree_dev = st.st_dev;
        job->tree_ino = st.st_ino;
        if (!tsearch(job, written, compare_tree_files)) gp_out_of_memory();

      }

      ++*parsed;
      break;

  }

  free(tree_path);
  return ret;

}

// Parse the jobs with `jobs` worker threads, and use the results in order.
// Return the exit code.
int gp_write_trees(job_t *list, size_t count, const char *trees_dir,
                   long jobs) {

  work_t work = {.jobs = list, .count = count};
  void  *written = NULL;
  size_t parsed = 0, skipped[REASON_COUNT] = {0};
  size_t started = 0;
  int    ret = 0;

  if ((size_t)jobs > count) jobs = (long)count;
  work.window = 4 * (size_t)jobs;
  pthread_t *threads = gp_xmalloc(((size_t)jobs) * sizeof(pthread_t));
  pthread_mutex_init(&work.lock, NULL);
  pthread_cond_init(&work.ready, NULL);
  pthread_cond_init(&work.space, NULL);

  for (; started < (size_t)jobs; ++started) {

    int error = pthread_create(&threads[started], NULL, worker, &work);
    if (error) {

      fprintf(stderr, "[-] Cannot start a parser thread: %s\n",
              strerror(error));
      ret = 2;
      break;

    }

  }

  for (size_t i = 0; !ret && i < count; ++i) {

    pthread_mutex_lock(&work.lock);
    while (!list[i].done)
      pthread_cond_wait(&work.ready, &work.lock);
    pthread_mutex_unlock(&work.lock);

    if (use_result(&list[i], trees_dir, &written, &parsed, skipped)) ret = 2;
    free(list[i].tree_buf);
    list[i].tree_buf = NULL;

    pthread_mutex_lock(&work.lock);
    work.consumed = i + 1;
    pthread_cond_broadcast(&work.space);
    pthread_mutex_unlock(&work.lock);

  }

  // On an error, the workers stop after the current file
  pthread_mutex_lock(&work.lock);
  work.stop = 1;
  pthread_cond_broadcast(&work.space);
  pthread_mutex_unlock(&work.lock);
  for (size_t i = 0; i < started; ++i)
    pthread_join(threads[i], NULL);
  free(threads);
  pthread_cond_destroy(&work.space);
  pthread_cond_destroy(&work.ready);
  pthread_mutex_destroy(&work.lock);
  // Each tdelete of the root job frees one node. The first member of a node
  // is the pointer to its job.
  while (written)
    tdelete(*(job_t **)written, &written, compare_tree_files);

  if (ret) return ret;
  printf("[+] Parsed %zu of %zu files; skipped: ", parsed, count);
  for (int r = 0; r < REASON_COUNT; ++r)
    printf("%s%s %zu", r ? ", " : "", reasons[r], skipped[r]);
  printf("\n");
  return 0;

}
