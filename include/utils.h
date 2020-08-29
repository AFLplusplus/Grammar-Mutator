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

#ifdef __cplusplus
}
#endif

#endif
