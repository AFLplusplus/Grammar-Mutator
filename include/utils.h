#ifndef __UTILS_H__
#define __UTILS_H__

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
 * @note This function breaks normal const rules and returns "non-const char *" just
 *       because that's what strstr() does!
 */
char * strrstr(const char * haystack, const char * needle);


#ifdef __cplusplus
}
#endif

#endif
