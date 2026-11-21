/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   grammar_parser-<name>: parse test cases with the parser of a .g4 build and
   write their tree files.

   Usage: grammar_parser-<name> [-j N] SEEDS_DIR TREES_DIR

   The tool parses each regular file in SEEDS_DIR and its subfolders, in the
   order of the relative paths. The tree file of a file goes to TREES_DIR. Its
   name is the base name of the file, with each space changed to "_" (AFL++
   does the same with queue names). Worker threads parse the files. The main
   thread writes the trees and prints the messages in file order, so the
   output does not depend on -j.
 */

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tree.h"

// The skip reasons, in the order of the summary
#define REASON_NOT_UTF8 0
#define REASON_SYNTAX_ERROR 1
#define REASON_PARSER_LIMIT 2
#define REASON_DUPLICATE_NAME 3
#define REASON_NO_MATCH 4
#define REASON_COUNT 5

static const char *reasons[REASON_COUNT] = {

    "not UTF-8", "syntax error", "parser limit", "duplicate name", "no match"};

// The result of the parse of one file
typedef enum {

  RESULT_TREE,
  RESULT_SKIP,
  RESULT_READ_ERROR,
  RESULT_THREAD_ERROR

} result_kind_t;

typedef struct {

  char *path;      // the path of the file (SEEDS_DIR/relative)
  char *relative;  // the path relative to SEEDS_DIR, with "/"

  // Set by a worker thread
  int           done;
  result_kind_t kind;
  int           reason;  // RESULT_SKIP: one of REASON_*
  int           error;   // RESULT_READ_ERROR: errno
  uint8_t      *tree_buf;
  size_t        tree_len;

  // Set by the main thread: the device and inode of the written tree file
  dev_t tree_dev;
  ino_t tree_ino;

} job_t;

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

// Print a file name. Each byte that is not part of valid UTF-8 prints as
// "\xNN".
static void print_name(FILE *stream, const char *name) {

  const unsigned char *s = (const unsigned char *)name;

  while (*s) {

    size_t len = 0;
    if (s[0] < 0x80) {

      len = 1;

    } else if (s[0] >= 0xC2 && s[0] <= 0xDF) {

      if (s[1] >= 0x80 && s[1] <= 0xBF) len = 2;

    } else if (s[0] >= 0xE0 && s[0] <= 0xEF) {

      unsigned char lo = s[0] == 0xE0 ? 0xA0 : 0x80;
      unsigned char hi = s[0] == 0xED ? 0x9F : 0xBF;
      if (s[1] >= lo && s[1] <= hi && s[2] >= 0x80 && s[2] <= 0xBF) len = 3;

    } else if (s[0] >= 0xF0 && s[0] <= 0xF4) {

      unsigned char lo = s[0] == 0xF0 ? 0x90 : 0x80;
      unsigned char hi = s[0] == 0xF4 ? 0x8F : 0xBF;
      if (s[1] >= lo && s[1] <= hi && s[2] >= 0x80 && s[2] <= 0xBF &&
          s[3] >= 0x80 && s[3] <= 0xBF)
        len = 4;

    }

    if (len) {

      fwrite(s, 1, len, stream);
      s += len;

    } else {

      fprintf(stream, "\\x%02x", s[0]);
      ++s;

    }

  }

}

// Print one message line: `before`, the file name, `after`, and the text of
// `error` (an errno value) if it is not 0
static void report(const char *before, const char *name, const char *after,
                   int error) {

  fputs(before, stderr);
  print_name(stderr, name);
  fputs(after, stderr);
  if (error) fprintf(stderr, ": %s", strerror(error));
  fputc('\n', stderr);

}

static void out_of_memory(void) {

  fprintf(stderr, "[-] Out of memory\n");
  exit(2);

}

static void *xmalloc(size_t size) {

  void *p = malloc(size ? size : 1);
  if (!p) out_of_memory();
  return p;

}

static void *xrealloc(void *p, size_t size) {

  p = realloc(p, size);
  if (!p) out_of_memory();
  return p;

}

static char *xstrdup(const char *s) {

  char *p = strdup(s);
  if (!p) out_of_memory();
  return p;

}

// Return "a/b". An empty `a` gives "b".
static char *join(const char *a, const char *b) {

  if (!*a) return xstrdup(b);
  size_t a_len = strlen(a);
  while (a_len > 1 && a[a_len - 1] == '/')
    --a_len;
  size_t b_len = strlen(b);
  char  *s = xmalloc(a_len + b_len + 2);
  memcpy(s, a, a_len);
  size_t pos = a_len;
  if (a[a_len - 1] != '/') s[pos++] = '/';
  memcpy(s + pos, b, b_len + 1);
  return s;

}

// Cut the absolute path `path` to its parent folder, as a name (no file
// system access). Return 0 if `path` is "/".
static int cut_to_parent(char *path) {

  size_t len = strlen(path);
  while (len > 1 && path[len - 1] == '/')
    --len;
  path[len] = '\0';
  char *slash = strrchr(path, '/');
  if (!slash || !strcmp(path, "/")) return 0;
  while (slash > path && slash[-1] == '/')
    --slash;
  if (slash == path)
    path[1] = '\0';
  else
    *slash = '\0';
  return 1;

}

// Compare the tree files of two jobs by device and inode
static int compare_tree_files(const void *a, const void *b) {

  const job_t *x = a, *y = b;
  if (x->tree_dev != y->tree_dev) return x->tree_dev < y->tree_dev ? -1 : 1;
  if (x->tree_ino != y->tree_ino) return x->tree_ino < y->tree_ino ? -1 : 1;
  return 0;

}

// Compare relative paths component by component, in byte order
static int compare_jobs(const void *a, const void *b) {

  const unsigned char *x = (const unsigned char *)((const job_t *)a)->relative;
  const unsigned char *y = (const unsigned char *)((const job_t *)b)->relative;
  while (*x && *x == *y) {

    ++x;
    ++y;

  }

  // The end of a path comes first, then "/", then the other bytes
  int cx = *x == '\0' ? 0 : *x == '/' ? 1 : *x + 1;
  int cy = *y == '\0' ? 0 : *y == '/' ? 1 : *y + 1;
  return cx - cy;

}

// Find the regular files in `seeds_dir` and its subfolders. A symbolic link
// to a file counts as a file. The tool does not follow a symbolic link to a
// folder. Return 0, or -1 with a message.
static int find_files(const char *seeds_dir, job_t **jobs_out,
                      size_t *count_out) {

  job_t *jobs = NULL;
  size_t count = 0, capacity = 0;
  char **folders = xmalloc(sizeof(char *));  // relative paths ("" = top)
  size_t folder_count = 1, folder_capacity = 1;
  int    ret = 0;

  folders[0] = xstrdup("");
  while (folder_count) {

    char *folder = folders[--folder_count];
    char *folder_path = *folder ? join(seeds_dir, folder) : xstrdup(seeds_dir);
    DIR  *dir = opendir(folder_path);
    if (!dir) {

      report("[-] Cannot read ", *folder ? folder : seeds_dir, "", errno);
      free(folder_path);
      free(folder);
      ret = -1;
      break;

    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {

      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
      char       *relative = join(folder, entry->d_name);
      char       *path = join(seeds_dir, relative);
      struct stat st;
      int         is_file = 0, is_folder = 0;
      if (!lstat(path, &st)) {

        if (S_ISLNK(st.st_mode))
          is_file = !stat(path, &st) && S_ISREG(st.st_mode);
        else if (S_ISREG(st.st_mode))
          is_file = 1;
        else if (S_ISDIR(st.st_mode))
          is_folder = 1;

      }

      if (is_file) {

        if (count == capacity) {

          capacity = capacity ? 2 * capacity : 64;
          jobs = xrealloc(jobs, capacity * sizeof(job_t));

        }

        memset(&jobs[count], 0, sizeof(job_t));
        jobs[count].path = path;
        jobs[count].relative = relative;
        ++count;

      } else if (is_folder) {

        if (folder_count == folder_capacity) {

          folder_capacity *= 2;
          folders = xrealloc(folders, folder_capacity * sizeof(char *));

        }

        folders[folder_count++] = relative;
        free(path);

      } else {

        free(relative);
        free(path);

      }

    }

    closedir(dir);
    free(folder_path);
    free(folder);

  }

  while (folder_count)
    free(folders[--folder_count]);
  free(folders);
  if (count) qsort(jobs, count, sizeof(job_t), compare_jobs);
  *jobs_out = jobs;
  *count_out = count;
  return ret;

}

// Read a file. Return its bytes, or NULL with errno set.
static uint8_t *read_file(const char *path, size_t *len_out) {

  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  size_t   capacity = 65536, len = 0;
  uint8_t *buf = malloc(capacity);
  while (buf) {

    len += fread(buf + len, 1, capacity - len, f);
    if (len < capacity) break;
    capacity *= 2;
    uint8_t *bigger = realloc(buf, capacity);
    if (!bigger) {

      free(buf);
      buf = NULL;

    } else {

      buf = bigger;

    }

  }

  int error = buf ? 0 : ENOMEM;
  if (buf && ferror(f)) error = errno ? errno : EIO;
  fclose(f);
  if (error) {

    free(buf);
    errno = error;
    return NULL;

  }

  *len_out = len;
  return buf;

}

// Read and parse the file of one job (in a worker thread)
static void parse_job(job_t *job) {

  size_t   len = 0;
  uint8_t *buf = read_file(job->path, &len);
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
  tree_serialize(tree);
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

// Remove the tree of an earlier run, which does not match the file now. A
// file that is not a tree stays. Return 0, or -1 with a message.
static int remove_old_tree(const char *tree_path, const char *relative) {

  struct stat st;
  if (stat(tree_path, &st) || !S_ISREG(st.st_mode)) return 0;

  size_t   len = 0;
  uint8_t *buf = read_file(tree_path, &len);
  if (buf) {

    tree_t *tree = tree_deserialize(buf, len);
    free(buf);
    if (!tree) {

      report("[!] ", tree_path, " is not a tree. It stays.", 0);
      return 0;

    }

    tree_free(tree);
    if (!unlink(tree_path)) return 0;

  }

  report("[-] Cannot remove the old tree of ", relative, "", errno);
  return -1;

}

// Write a tree file. Return 0, or -1 with errno set.
static int write_file(const char *path, const uint8_t *buf, size_t len) {

  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  int ok = fwrite(buf, 1, len, f) == len;
  int error = errno;
  if (fclose(f)) {

    ok = 0;
    error = errno;

  }

  errno = error;
  return ok ? 0 : -1;

}

// Use the result of one job: write the tree, or report the skip. `written`
// is the search tree of the jobs with a tree file. Return 0, or -1 with a
// message.
static int use_result(job_t *job, const char *trees_dir, void **written,
                      size_t *parsed, size_t *skipped) {

  // AFL++ changes each space in a queue name to "_" (no_spaces() in
  // afl-fuzz-init.c), and the mutator looks for the tree with that name.
  const char *slash = strrchr(job->relative, '/');
  char       *name = xstrdup(slash ? slash + 1 : job->relative);
  for (char *c = name; *c; ++c)
    if (*c == ' ') *c = '_';
  char *tree_path = join(trees_dir, name);
  free(name);

  // Two tree names are the same name if the file system gives them one file,
  // for example A.json and a.json on macOS
  struct stat st;
  int         ret = 0;
  if (!stat(tree_path, &st)) {

    job->tree_dev = st.st_dev;
    job->tree_ino = st.st_ino;
    if (tfind(job, written, compare_tree_files)) {

      job->kind = RESULT_SKIP;
      job->reason = REASON_DUPLICATE_NAME;

    }

  }

  switch (job->kind) {

    case RESULT_READ_ERROR:
      report("[-] Cannot read ", job->relative, "", job->error);
      ret = -1;
      break;

    case RESULT_THREAD_ERROR:
      report("[-] Cannot start the parser thread for ", job->relative, "", 0);
      ret = -1;
      break;

    case RESULT_SKIP:
      ++skipped[job->reason];
      fputs("[!] ", stderr);
      print_name(stderr, job->relative);
      fprintf(stderr, ": %s\n", reasons[job->reason]);
      if (job->reason != REASON_DUPLICATE_NAME)
        ret = remove_old_tree(tree_path, job->relative);
      break;

    case RESULT_TREE:
      if (write_file(tree_path, job->tree_buf, job->tree_len)) {

        report("[-] Cannot write the tree of ", job->relative, "", errno);
        ret = -1;
        break;

      }

      if (!stat(tree_path, &st)) {

        job->tree_dev = st.st_dev;
        job->tree_ino = st.st_ino;
        if (!tsearch(job, written, compare_tree_files)) out_of_memory();

      }

      ++*parsed;
      break;

  }

  free(tree_path);
  return ret;

}

// Stop if TREES_DIR is SEEDS_DIR or a folder in it. The check compares the
// device and inode of the folders, not their names. So it also finds the same
// folder under a name with other case, a symbolic link, or a firmlink.
// Return 0, or -1 with a message.
static int check_folders(const char *seeds_dir, const char *trees_dir) {

  struct stat seeds, st;
  char       *folder = NULL, *real = NULL;
  int         same = 0, error = 0;

  if (stat(seeds_dir, &seeds)) {

    error = errno;

  } else if (trees_dir[0] == '/') {

    folder = xstrdup(trees_dir);

  } else {

    char *cwd = getcwd(NULL, 0);
    if (!cwd) {

      error = errno;

    } else {

      folder = join(cwd, trees_dir);
      free(cwd);

    }

  }

  // The folders that do not exist yet cannot be SEEDS_DIR. Start at the
  // deepest folder that exists, and walk up its real path.
  while (!error && stat(folder, &st)) {

    if (errno != ENOENT && errno != ENOTDIR)
      error = errno;
    else if (!cut_to_parent(folder))
      break;

  }

  if (!error && !(real = realpath(folder, NULL))) error = errno;
  while (!error && !same) {

    if (stat(real, &st))
      error = errno;
    else if (st.st_dev == seeds.st_dev && st.st_ino == seeds.st_ino)
      same = 1;
    else if (!cut_to_parent(real))
      break;

  }

  free(real);
  free(folder);
  if (error) {

    report("[-] Cannot check TREES_DIR ", trees_dir, "", error);
    return -1;

  }

  if (same) {

    fprintf(stderr,
            "[-] TREES_DIR must not be SEEDS_DIR or a folder in SEEDS_DIR\n");
    return -1;

  }

  return 0;

}

// Make a folder and its parents (as "mkdir -p"). Return 0, or -1 with errno
// set. An empty path gives -1 with ENOENT.
static int make_folders(const char *path) {

  char *p = xstrdup(path);
  for (char *c = p; *c; ++c) {

    // A "/" at the start is the root folder, not a parent to make
    if (*c != '/' || c == p) continue;
    *c = '\0';
    if (mkdir(p, 0777) && errno != EEXIST) {

      free(p);
      return -1;

    }

    *c = '/';

  }

  struct stat st;
  int         ret = 0;
  if (mkdir(p, 0777)) {

    if (errno != EEXIST) {

      ret = -1;

    } else if (stat(p, &st) || !S_ISDIR(st.st_mode)) {

      errno = EEXIST;
      ret = -1;

    }

  }

  free(p);
  return ret;

}

// Parse the jobs with `jobs` worker threads, and use the results in order.
// Return the exit code.
static int write_trees(job_t *list, size_t count, const char *trees_dir,
                       long jobs) {

  work_t work = {.jobs = list, .count = count};
  void  *written = NULL;
  size_t parsed = 0, skipped[REASON_COUNT] = {0};
  size_t started = 0;
  int    ret = 0;

  if ((size_t)jobs > count) jobs = (long)count;
  work.window = 4 * (size_t)jobs;
  pthread_t *threads = xmalloc(((size_t)jobs) * sizeof(pthread_t));
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

static void usage(FILE *stream, const char *program) {

  const char *slash = strrchr(program, '/');
  fprintf(stream, "Usage: %s [-j N] SEEDS_DIR TREES_DIR\n",
          slash ? slash + 1 : program);

}

int main(int argc, char *argv[]) {

  long jobs = sysconf(_SC_NPROCESSORS_ONLN);
  if (jobs < 1) jobs = 1;

  int option;
  opterr = 0;
  while ((option = getopt(argc, argv, "hj:")) != -1) {

    if (option == 'h') {

      usage(stdout, argv[0]);
      printf(
          "Parse the test cases in SEEDS_DIR and write their tree files to "
          "TREES_DIR.\n"
          "  -j N  the number of parser threads (default: the number of "
          "CPUs, %ld here)\n",
          jobs);
      return 0;

    }

    if (option == 'j') {

      char *end;
      errno = 0;
      jobs = strtol(optarg, &end, 10);
      if (errno || end == optarg || *end || jobs < 1) {

        fprintf(stderr, "[-] -j must be a number of 1 or more: %s\n", optarg);
        usage(stderr, argv[0]);
        return 2;

      }

      continue;

    }

    if (optopt == 'j')
      fprintf(stderr, "[-] -j needs a number\n");
    else
      fprintf(stderr, "[-] Unknown option: -%c\n", optopt);
    usage(stderr, argv[0]);
    return 2;

  }

  if (argc - optind != 2) {

    fprintf(stderr, "[-] Give SEEDS_DIR and TREES_DIR\n");
    usage(stderr, argv[0]);
    return 2;

  }

  const char *seeds_dir = argv[optind];
  const char *trees_dir = argv[optind + 1];
  if (!seeds_dir[0] || !trees_dir[0]) {

    fprintf(stderr, "[-] %s must not be empty\n",
            seeds_dir[0] ? "TREES_DIR" : "SEEDS_DIR");
    usage(stderr, argv[0]);
    return 2;

  }

  struct stat st;
  if (stat(seeds_dir, &st) || !S_ISDIR(st.st_mode)) {

    report("[-] Not a folder: ", seeds_dir, "", 0);
    return 2;

  }

  if (check_folders(seeds_dir, trees_dir)) return 2;

  job_t *list = NULL;
  size_t count = 0;
  int    ret = 2;
  if (!find_files(seeds_dir, &list, &count)) {

    if (make_folders(trees_dir)) {

      report("[-] Cannot make the folder ", trees_dir, "", errno);

    } else {

      ret = write_trees(list, count, trees_dir, jobs);

    }

  }

  for (size_t i = 0; i < count; ++i) {

    free(list[i].path);
    free(list[i].relative);
    free(list[i].tree_buf);

  }

  free(list);
  return ret;

}
