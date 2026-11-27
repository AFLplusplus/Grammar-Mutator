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

#include <stdlib.h>
#include <string.h>

#include "mutants.h"

#define MAX_BLOCK 32
#define MAX_OPERATIONS 16

void bench_rng_seed(bench_rng_t *rng, uint64_t seed) {

  // One splitmix64 step
  uint64_t z = seed + 0x9e3779b97f4a7c15ULL;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  z ^= z >> 31;
  // The state of xorshift64* must not be 0
  rng->state = z ? z : 0x9e3779b97f4a7c15ULL;

}

uint64_t bench_rng_next(bench_rng_t *rng) {

  uint64_t x = rng->state;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  rng->state = x;
  return x * 0x2545f4914f6cdd1dULL;

}

// A number below n (n must be above 0)
static size_t below(bench_rng_t *rng, size_t n) {

  return (size_t)(bench_rng_next(rng) % (uint64_t)n);

}

// A block size from 1 to MAX_BLOCK, not above max
static size_t block_size(bench_rng_t *rng, size_t max) {

  size_t n = 1 + below(rng, MAX_BLOCK);
  return n < max ? n : max;

}

uint8_t *bench_make_mutant(bench_rng_t *rng, const bench_seed_t *seeds,
                           size_t count, size_t *len) {

  const bench_seed_t *base = &seeds[below(rng, count)];
  size_t              used =
      base->len < BENCH_MUTANT_MAX_LEN ? base->len : BENCH_MUTANT_MAX_LEN;
  // The capacity leaves room for most insertions. An operation that needs
  // more room grows the buffer.
  size_t   cap = used + 2 * MAX_BLOCK * MAX_OPERATIONS + 1;
  uint8_t *buf = malloc(cap);
  if (!buf) return NULL;
  if (used > 0) memcpy(buf, base->buf, used);
  size_t operations = 1 + below(rng, MAX_OPERATIONS);
  for (size_t k = 0; k < operations; ++k) {

    switch (below(rng, 6)) {

      case 0:  // flip one bit
        if (used > 0) buf[below(rng, used)] ^= (uint8_t)(1u << below(rng, 8));
        break;
      case 1:  // set one byte to a random value
        if (used > 0) buf[below(rng, used)] = (uint8_t)bench_rng_next(rng);
        break;
      case 2: {  // delete a block

        if (used == 0) break;
        size_t n = block_size(rng, used);
        size_t pos = below(rng, used - n + 1);
        memmove(buf + pos, buf + pos + n, used - pos - n);
        used -= n;
        break;

      }

      case 3: {  // insert a block of random bytes

        if (used >= BENCH_MUTANT_MAX_LEN) break;
        size_t n = block_size(rng, BENCH_MUTANT_MAX_LEN - used);
        size_t pos = below(rng, used + 1);
        // Grow the buffer if the insertion does not fit
        if (used + n > cap) {

          size_t   bigger_cap = (used + n) * 2;
          uint8_t *bigger = realloc(buf, bigger_cap);
          if (!bigger) {

            free(buf);
            return NULL;

          }

          buf = bigger;
          cap = bigger_cap;

        }

        memmove(buf + pos + n, buf + pos, used - pos);
        for (size_t i = 0; i < n; ++i)
          buf[pos + i] = (uint8_t)bench_rng_next(rng);
        used += n;
        break;

      }

      case 4: {  // copy a block of the input to another place in it

        if (used == 0) break;
        size_t n = block_size(rng, used);
        size_t from = below(rng, used - n + 1);
        size_t to = below(rng, used - n + 1);
        memmove(buf + to, buf + from, n);
        break;

      }

      default: {  // splice: the head of this input and the tail of a seed

        const bench_seed_t *other = &seeds[below(rng, count)];
        size_t              head = below(rng, used + 1);
        size_t              tail = below(rng, other->len + 1);
        size_t              n = other->len - tail;
        if (head + n > BENCH_MUTANT_MAX_LEN) n = BENCH_MUTANT_MAX_LEN - head;
        if (head + n > cap) {

          size_t   bigger_cap = (head + n) * 2;
          uint8_t *bigger = realloc(buf, bigger_cap);
          if (!bigger) {

            free(buf);
            return NULL;

          }

          buf = bigger;
          cap = bigger_cap;

        }

        if (n > 0) memcpy(buf + head, other->buf + tail, n);
        used = head + n;
        break;

      }

    }

  }

  *len = used;
  return buf;

}
