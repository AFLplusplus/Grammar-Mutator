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

#ifndef BENCH_MUTANTS_H
#define BENCH_MUTANTS_H

#include <stddef.h>
#include <stdint.h>

// A small random number generator (xorshift64*). The mutants do not depend
// on the library RNG, so they are the same on every platform.
typedef struct {

  uint64_t state;

} bench_rng_t;

// Start the generator from a seed. A mixing step makes seed 0 work.
void bench_rng_seed(bench_rng_t *rng, uint64_t seed);

// The next 64 random bits
uint64_t bench_rng_next(bench_rng_t *rng);

// The bytes of one seed file. The caller owns the bytes.
typedef struct {

  const uint8_t *buf;
  size_t         len;

} bench_seed_t;

// The size limit of a mutant in bytes
#define BENCH_MUTANT_MAX_LEN ((size_t)1 << 20)

// Make one mutant of a random seed. Return a new buffer (use free) and set
// "len". Return NULL if memory runs out. "count" must be at least 1.
uint8_t *bench_make_mutant(bench_rng_t *rng, const bench_seed_t *seeds,
                           size_t count, size_t *len);

#endif
