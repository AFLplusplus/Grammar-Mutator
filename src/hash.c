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

#include "hash.h"

#include "xxh3.h"

#ifdef DEBUG_BUILD
uint64_t hash64(uint8_t *key, uint32_t len, uint64_t seed) {

#else
inline uint64_t hash64(uint8_t *key, uint32_t len, uint64_t seed) {

#endif
  return XXH64(key, len, seed);

}

