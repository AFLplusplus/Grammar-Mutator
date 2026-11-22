/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The parts of grammar_parser-<name> (grammar_parser.c): the folder walk and
   the file I/O (grammar_parser_files.c), and the worker threads
   (grammar_parser_work.c).

   The shared functions have the prefix "gp_". The program links the mutator
   library, and a global name in the program hides the same name in the
   library.
 */

#ifndef __GRAMMAR_PARSER_INTERNAL_H__
#define __GRAMMAR_PARSER_INTERNAL_H__

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

// The skip reasons, in the order of the summary
#define REASON_NOT_UTF8 0
#define REASON_SYNTAX_ERROR 1
#define REASON_PARSER_LIMIT 2
#define REASON_DUPLICATE_NAME 3
#define REASON_NO_MATCH 4
#define REASON_COUNT 5

// The result of the parse of one file
typedef enum {

  RESULT_TREE,
  RESULT_SKIP,
  RESULT_READ_ERROR,
  RESULT_THREAD_ERROR,
  RESULT_MEMORY_ERROR

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

// grammar_parser_files.c: messages and memory
void  gp_print_name(FILE *stream, const char *name);
void  gp_report(const char *before, const char *name, const char *after,
                int error);
void  gp_out_of_memory(void);
void *gp_xmalloc(size_t size);
void *gp_xrealloc(void *p, size_t size);
char *gp_xstrdup(const char *s);
char *gp_join(const char *a, const char *b);

// grammar_parser_files.c: the folders and the files
int gp_find_files(const char *seeds_dir, job_t **jobs_out, size_t *count_out);
uint8_t *gp_read_file(const char *path, size_t *len_out);
int      gp_remove_old_tree(const char *tree_path, const char *relative);
int      gp_write_file(const char *path, const uint8_t *buf, size_t len);
int      gp_check_folders(const char *seeds_dir, const char *trees_dir);
int      gp_make_folders(const char *path);

// grammar_parser_work.c: parse the files and write the trees
int gp_write_trees(job_t *list, size_t count, const char *trees_dir, long jobs);

#endif
