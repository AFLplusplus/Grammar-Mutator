#ifndef __HASH_H__
#define __HASH_H__

#include <stdint.h>

// TODO: may replace this
#define HASH_SEED 0xdeadbeef

#ifdef __cplusplus
extern "C" {
#endif

// Hash functions
uint64_t hash64(uint8_t *key, uint32_t len, uint64_t seed);

#ifdef __cplusplus
}
#endif

#endif