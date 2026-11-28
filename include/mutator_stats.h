#ifndef __MUTATOR_STATS_H__
#define __MUTATOR_STATS_H__

#include <stdbool.h>
#include <stdint.h>

#include "tree.h"

#ifdef __cplusplus
extern "C" {

#endif

// The largest value of GRAMMAR_MUTATOR_STATS_SECONDS (one day)
#define MUTATOR_STATS_MAX_SECONDS 86400
// The name of the statistics file in the output folder of an AFL++ instance
#define MUTATOR_STATS_FILE_NAME "grammar_mutator_stats"

// The statistics file of one mutator: the parse counts, the prediction states
// of the parse cache, and the heap in use. All functions accept NULL and then
// do nothing. The functions draw no random numbers and change no cache.
typedef struct mutator_stats mutator_stats_t;

// Return the interval in seconds from GRAMMAR_MUTATOR_STATS_SECONDS, or 0 for
// no file. Unset, empty, and "0" give 0. Any value other than a whole number
// from 0 to MUTATOR_STATS_MAX_SECONDS prints a warning and gives 0.
uint32_t mutator_stats_seconds_from_env(void);

// Create the state for a line every "seconds" seconds (1 to
// MUTATOR_STATS_MAX_SECONDS). Return NULL if the allocation fails.
mutator_stats_t *mutator_stats_create(uint32_t seconds);

// Count one parse attempt that read the test case file
void mutator_stats_count_parse(mutator_stats_t *stats, bool got_tree,
                               parse_status_t status);

// Open the file in the folder "instance_dir", write the header if the file is
// empty, and write the first line. "now_ns" is the monotonic time in ns. Only
// the first call does work, also if the open fails.
void mutator_stats_open(mutator_stats_t *stats, const char *instance_dir,
                        uint64_t now_ns);

// Write a line if the file is open and the interval passed since the last
// line. "now_ns" is the monotonic time in ns.
void mutator_stats_tick(mutator_stats_t *stats, uint64_t now_ns);

// Write the last line if the file is open, close the file, and free the state
void mutator_stats_free(mutator_stats_t *stats);

#ifdef __cplusplus

}

#endif

#endif
