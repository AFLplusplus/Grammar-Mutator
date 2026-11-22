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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "grammar_parser_internal.h"

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

    gp_report("[-] Not a folder: ", seeds_dir, "", 0);
    return 2;

  }

  if (gp_check_folders(seeds_dir, trees_dir)) return 2;

  job_t *list = NULL;
  size_t count = 0;
  int    ret = 2;
  if (!gp_find_files(seeds_dir, &list, &count)) {

    if (gp_make_folders(trees_dir)) {

      gp_report("[-] Cannot make the folder ", trees_dir, "", errno);

    } else {

      ret = gp_write_trees(list, count, trees_dir, jobs);

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
