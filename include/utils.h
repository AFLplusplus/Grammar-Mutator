#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// File/directory operations
/**
 * This function first checks whether the given directory path exists. It will
 * create the directory, if the given path does not exist.
 * @param  path The directory path
 * @return      True, if the directory exists or is successfully created;
 *              otherwise, False.
 */
bool create_directory(const char *path);

/**
 * Thie function removes the directory and recursively deletes all files and
 * subdirectories under this directory.
 * @param  path The directory path
 * @return      True, if everything is successfully removed; otherwise, False.
 */
bool remove_directory(const char *path);

// String operations
/**
 * This function is the same as `strstr` except it finds the LAST occurance
 * of the \p needle in the \p haystack instead of the FIRST.
 *
 * @param haystack The string in which to find the \p needle
 * @param needle   The substring to be found inside of \p haystack
 * @return         A pointer to the LAST substring location in the \p haystack,
 *                 or NULL if no match was found.
 *
 * @note This function breaks normal const rules and returns "non-const char *"
 * just because that's what strstr() does!
 */
char *strrstr(const char *haystack, const char *needle);

// Random number generators
/* 64bit arch MACRO */
#if (defined(__x86_64__) || defined(__arm64__) || defined(__aarch64__))
#define WORD_SIZE_64 1
#endif /* (defined(__x86_64__) || defined(__arm64__) || defined(__aarch64__)) */

#ifdef WORD_SIZE_64
#define RANDOM_RETURN uint64_t
#else
#define RANDOM_RETURN uint32_t
#endif /* WORD_SIZE_64 */

/**
 * This function sets the random seed for Romu random number generators
 * @param seed The random seed
 * @refitem    afl-performance.c from AFLplusplus/AFLplusplus
 *             https://github.com/AFLplusplus/AFLplusplus/blob/stable/src/afl-performance.c#L30-L39
 */
void random_set_seed(uint64_t seed);

/**
 * This function is a wrapper of Romu random number generators
 * @return  A random number
 * @refitem "Romu - Fine random number generators"
 *          https://www.romu-random.org
 */
RANDOM_RETURN random_next();

/**
 * This function generates a random number, ranging from [0, limit).
 * @param limit The maximum limit of the generated random number
 * @return      A random number that is smaller than `limit`
 * @refitem     afl-fuzz.h from AFLplusplus/AFLplusplus
 *              https://github.com/AFLplusplus/AFLplusplus/blob/stable/include/afl-fuzz.h#L1157-L1185
 */
uint32_t random_below(uint32_t limit);

#ifdef __cplusplus
}
#endif

#endif
