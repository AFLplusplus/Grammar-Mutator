#ifndef __PARSE_BUDGET_H__
#define __PARSE_BUDGET_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {

#endif

// The default smallest time limit of one parse: 50 ms (PARSE_TIME_BUDGET_MS)
#define PARSE_BUDGET_MIN_NS UINT64_C(50000000)
// The limit is this factor times the median parse speed
#define PARSE_BUDGET_FACTOR UINT64_C(50)
// The number of records before the budget gives a limit
#define PARSE_BUDGET_WARMUP 32
// The number of recent records that the median uses
#define PARSE_BUDGET_WINDOW 255

// The record of recent parse speeds. A budget is not thread-safe: use one
// budget from one thread, or lock around every call.
typedef struct parse_budget parse_budget_t;

// Create an empty budget. Return NULL if the allocation fails.
parse_budget_t *parse_budget_create(void);
// Free a budget. NULL is allowed.
void parse_budget_free(parse_budget_t *budget);

// Set the smallest time limit in ns (default PARSE_BUDGET_MIN_NS). NULL is
// allowed.
void parse_budget_set_min_ns(parse_budget_t *budget, uint64_t min_ns);

// Return the time limit in ns for a parse of "tokens" tokens.
// Return 0 for "no limit" (warm-up).
uint64_t parse_budget_limit_ns(const parse_budget_t *budget, size_t tokens);

// Return the deadline of a parse that starts at "start_ns" with the time limit
// "limit_ns". Return 0 (no deadline) for a limit of 0, and UINT64_MAX if the
// sum is too large.
uint64_t parse_budget_deadline_ns(uint64_t start_ns, uint64_t limit_ns);

// Record one successful parse. A token count of 0 counts as 1.
void parse_budget_record(parse_budget_t *budget, uint64_t elapsed_ns,
                         size_t tokens);

// Read a monotonic clock in ns
uint64_t parse_budget_now_ns(void);

// Read PARSE_TIME_BUDGET_MS and return the smallest time limit in ns. Unset or
// empty: PARSE_BUDGET_MIN_NS. Digits only: that number of ms (0 turns the limit
// off). Anything else: a warning on stderr and PARSE_BUDGET_MIN_NS.
uint64_t parse_budget_min_ns_from_env(void);

#ifdef __cplusplus

}

#endif

#endif
