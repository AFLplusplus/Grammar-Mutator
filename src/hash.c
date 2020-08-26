#include "hash.h"

#include "xxh3.h"

#ifdef DEBUG_BUILD
uint64_t hash64(uint8_t *key, uint32_t len, uint64_t seed) {
#else
inline uint64_t hash64(uint8_t *key, uint32_t len, uint64_t seed) {
#endif
  return XXH64(key, len, seed);
}
