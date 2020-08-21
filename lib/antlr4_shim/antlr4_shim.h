#ifndef __ANTLR4_SHIM_H__
#define __ANTLR4_SHIM_H__

#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse the given buffer to construct a parsing tree
 * @param  data_buf  The buffer of a test case
 * @param  data_size The size of the buffer
 * @return           A newly created tree
 */
tree_t *tree_from_buf(const uint8_t *data_buf, size_t data_size);

#ifdef __cplusplus
}
#endif

#endif
