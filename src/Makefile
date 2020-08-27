#
# american fuzzy lop++ - grammar mutator
# --------------------------------------
#
# Written by Shengtuo Hu
#
# Copyright 2020 AFLplusplus Project. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# A grammar-based custom mutator written for GSoC '20.
#

TARGETS = libgrammarmutator.so grammar_generator benchmark/benchmark

LIB_SRC_FILES = chunk_store.c f1_c_fuzz.c grammar_mutator.c list.c tree.c tree_mutation.c tree_trimming.c hash.c
GEN_SRC_FILES = grammar_generator.c
BENCHMARK_SRC_FILES = benchmark/benchmark.c

LIB_OBJS = $(LIB_SRC_FILES:.c=.o)
GEN_OBJS = $(GEN_SRC_FILES:.c=.o)
BENCHMARK_OBJS = $(BENCHMARK_SRC_FILES:.c=.o)
OBJS = $(LIB_OBJS) $(GEN_OBJS) $(BENCHMARK_OBJS)

C_FLAGS =
C_DEFINES =
C_INCLUDES = -I../include -I../third_party/rxi_map -I../third_party/barrust_set -I../third_party/Cyan4973_xxHash

RXI_MAP_LIB = $(realpath ../third_party/rxi_map/librxi_map.a)
BARRUST_SET_LIB = $(realpath ../third_party/barrust_set/libbarrust_set.a)
ANTLR4_CXX_RUNTIME_LIB = $(realpath ../third_party/antlr4-cpp-runtime/libantlr4-runtime.a)
ANTLR4_SHIM_LIB = $(realpath ../lib/antlr4_shim/libantlr4_shim.a)
XXHASH_LIB = $(realpath ../third_party/Cyan4973_xxHash/libxxhash.a)

LIBS = $(RXI_MAP_LIB) $(BARRUST_SET_LIB) $(ANTLR4_SHIM_LIB) $(ANTLR4_CXX_RUNTIME_LIB) $(XXHASH_LIB)
LDFLAGS = $(LIBS)

ifdef ENABLE_DEBUG
C_FLAGS += -g -O0
C_DEFINES += -DDEBUG_BUILD
else
C_FLAGS += -O3
endif

.PHONY: all
all: $(TARGETS)

libgrammarmutator.so: $(LIB_OBJS)
	$(CXX) -fPIC $(C_FLAGS) -shared -Wl,-soname,libgrammarmutator.so -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(C_DEFINES) $(C_INCLUDES) -fPIC $(C_FLAGS) -o $@ -c $<

grammar_generator.o: grammar_generator.c
	$(CC) $(C_DEFINES) -I../include $(C_FLAGS) -o $@ -c $<

grammar_generator: $(GEN_OBJS) libgrammarmutator.so
	$(CXX) $(C_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ./) libgrammarmutator.so

benchmark/benchmark.o: benchmark/benchmark.c
	$(CC) $(C_DEFINES) -I../include $(C_FLAGS) -o $@ -c $<

benchmark/benchmark: $(BENCHMARK_OBJS) libgrammarmutator.so
	$(CXX) $(C_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ./) libgrammarmutator.so -lm

.PHONY: clean
clean:
	@rm -f $(OBJS)
	@rm -f $(TARGETS)
