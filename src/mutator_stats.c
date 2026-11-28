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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mutator_stats.h"
#include "utils.h"

#define NS_PER_SECOND UINT64_C(1000000000)

struct mutator_stats {

  uint64_t interval_ns;
  uint64_t next_ns;  // the time of the next line
  bool     opened;   // true after the first open call, also if it failed
  FILE    *file;     // NULL if the open failed or did not occur

  uint64_t parses, failed_parses, limit_parses;

};

uint32_t mutator_stats_seconds_from_env(void) {

  const char *ptr = getenv("GRAMMAR_MUTATOR_STATS_SECONDS");
  if (!ptr || !*ptr) return 0;

  uint32_t seconds = 0;
  for (const char *p = ptr; *p; ++p) {

    if (*p < '0' || *p > '9' ||
        seconds > (MUTATOR_STATS_MAX_SECONDS - (uint32_t)(*p - '0')) / 10) {

      fprintf(stderr,
              "[grammar mutator] warning: GRAMMAR_MUTATOR_STATS_SECONDS must "
              "be a whole number of seconds from 0 to 86400. The mutator "
              "writes no statistics file.\n");
      return 0;

    }

    seconds = seconds * 10 + (uint32_t)(*p - '0');

  }

  return seconds;

}

mutator_stats_t *mutator_stats_create(uint32_t seconds) {

  mutator_stats_t *stats = calloc(1, sizeof(mutator_stats_t));
  if (!stats) return NULL;

  stats->interval_ns = (uint64_t)seconds * NS_PER_SECOND;
  return stats;

}

void mutator_stats_count_parse(mutator_stats_t *stats, bool got_tree,
                               parse_status_t status) {

  if (!stats) return;

  ++stats->parses;
  if (!got_tree) ++stats->failed_parses;
  if (status == PARSE_LIMIT) ++stats->limit_parses;

}

// Write one line with the current values
static void write_line(mutator_stats_t *stats) {

  long long parser_states = -1, lexer_states = -1;
  size_t    parser_count = 0, lexer_count = 0;
  if (tree_parser_cache_states(&parser_count, &lexer_count)) {

    parser_states = (long long)parser_count;
    lexer_states = (long long)lexer_count;

  }

  fprintf(stats->file, "%lld, %llu, %llu, %llu, %lld, %lld, %ld\n",
          (long long)time(NULL), (unsigned long long)stats->parses,
          (unsigned long long)stats->failed_parses,
          (unsigned long long)stats->limit_parses, parser_states, lexer_states,
          heap_in_use_kb());
  fflush(stats->file);

}

void mutator_stats_open(mutator_stats_t *stats, const char *instance_dir,
                        uint64_t now_ns) {

  if (!stats || stats->opened) return;
  stats->opened = true;

  char path[PATH_MAX];
  int  len = snprintf(path, sizeof(path), "%s/%s", instance_dir,
                      MUTATOR_STATS_FILE_NAME);
  if (len < 0 || (size_t)len >= sizeof(path)) {

    errno = ENAMETOOLONG;

  } else {

    stats->file = fopen(path, "a");

  }

  if (!stats->file) {

    fprintf(stderr,
            "[grammar mutator] warning: cannot open the statistics file %s "
            "(%s). The mutator writes no statistics file.\n",
            path, strerror(errno));
    return;

  }

  // Write the header only into an empty file. A resumed run appends to the
  // file of the old run.
  if (fseek(stats->file, 0, SEEK_END) == 0 && ftell(stats->file) == 0)
    fputs(
        "# unix_time, parses, failed_parses, limit_parses, parser_states, "
        "lexer_states, heap_kb\n",
        stats->file);
  write_line(stats);
  stats->next_ns = now_ns + stats->interval_ns;

}

void mutator_stats_tick(mutator_stats_t *stats, uint64_t now_ns) {

  if (!stats || !stats->file || now_ns < stats->next_ns) return;
  write_line(stats);
  // Count from this line, so that a late check gives no burst of lines
  stats->next_ns = now_ns + stats->interval_ns;

}

void mutator_stats_free(mutator_stats_t *stats) {

  if (!stats) return;

  if (stats->file) {

    write_line(stats);
    fclose(stats->file);

  }

  free(stats);

}
