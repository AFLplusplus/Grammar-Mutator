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

BUILD = yes
ifeq "$(filter $(MAKECMDGOALS),clean)" "clean"
override BUILD = no
endif

ifeq ($(BUILD),yes)

# Check GRAMMAR_FILENAME
ifndef GRAMMAR_FILENAME
$(error Please specify the grammar filename)
endif
ifeq (,$(GRAMMAR_FILENAME))
$(error Please specify the grammar filename)
endif

endif

GRAMMAR_MUTATOR_LIB = libgrammarmutator-$(GRAMMAR_FILENAME).so
GRAMMAR_GENERATOR_PROM = grammar_generator-$(GRAMMAR_FILENAME)
BENCH_PROM = benchmark/benchmark-$(GRAMMAR_FILENAME)
TARGETS = $(GRAMMAR_MUTATOR_LIB) $(GRAMMAR_GENERATOR_PROM) $(BENCH_PROM)
# A .g4 build (G4_BUILD) also makes grammar_parser-<name>
GRAMMAR_PARSER_PROM = grammar_parser-$(GRAMMAR_FILENAME)
ifdef G4_BUILD
TARGETS += $(GRAMMAR_PARSER_PROM)
endif

LIB_SRC_FILES = chunk_store.c f1_c_fuzz.c grammar_mutator.c list.c parse_budget.c tree.c tree_mutation.c tree_trimming.c utils.c
GEN_SRC_FILES = grammar_generator.c
PARSER_SRC_FILES = grammar_parser.c grammar_parser_files.c grammar_parser_work.c
BENCHMARK_SRC_FILES = benchmark/benchmark.c benchmark/mutants.c

LIB_OBJS = $(LIB_SRC_FILES:.c=.o)
GEN_OBJS = $(GEN_SRC_FILES:.c=.o)
PARSER_OBJS = $(PARSER_SRC_FILES:.c=.o)
BENCHMARK_OBJS = $(BENCHMARK_SRC_FILES:.c=.o)
OBJS = $(LIB_OBJS) $(GEN_OBJS) $(PARSER_OBJS) $(BENCHMARK_OBJS)

C_FLAGS = $(C_FLAGS_OPT)
# Each compile also writes a .d file with the headers of the object. So a
# change in a header makes the objects that include it again.
DEP_FLAGS = -MMD -MP
C_DEFINES =
C_INCLUDES = -I../include -I../third_party/rxi_map -I../third_party/Cyan4973_xxHash

RXI_MAP_LIB = $(realpath ../third_party/rxi_map/librxi_map.a)
ANTLR4_CXX_RUNTIME_LIB = $(realpath ../third_party/antlr4-cpp-runtime/libantlr4-runtime.a)
ANTLR4_SHIM_LIB = $(realpath ../lib/antlr4_shim/libantlr4_shim.a)
XXHASH_LIB = $(realpath ../third_party/Cyan4973_xxHash/libxxhash.a)

LIBS = $(RXI_MAP_LIB) $(ANTLR4_SHIM_LIB) $(ANTLR4_CXX_RUNTIME_LIB) $(XXHASH_LIB)

# The linker option that names the shared library. The macOS linker does not
# know the GNU option -soname.
ifeq "$(shell uname -s)" "Darwin"
SONAME_FLAG = -Wl,-install_name,@rpath/$(GRAMMAR_MUTATOR_LIB)
else
SONAME_FLAG = -Wl,-soname,$(GRAMMAR_MUTATOR_LIB)
endif
LDFLAGS = $(LIBS)
ifdef G4_BUILD
# The parser of a .g4 build runs each parse in its own thread
LDFLAGS += -pthread
endif

ifdef ENABLE_DEBUG
C_FLAGS += -g -O0
C_DEFINES += -DDEBUG_BUILD
else
C_FLAGS += -O3
endif

.PHONY: all
all: $(TARGETS)

# A new parser in the shim library also makes the library again
$(GRAMMAR_MUTATOR_LIB): $(LIB_OBJS) $(ANTLR4_SHIM_LIB)
	$(CXX) -fPIC $(C_FLAGS) -shared $(SONAME_FLAG) -o $@ $(LIB_OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(C_DEFINES) $(C_INCLUDES) -fPIC $(C_FLAGS) $(DEP_FLAGS) -o $@ -c $<

grammar_generator.o: grammar_generator.c
	$(CC) $(C_DEFINES) -I../include $(C_FLAGS) $(DEP_FLAGS) -o $@ -c $<

$(GRAMMAR_GENERATOR_PROM): $(GEN_OBJS) $(GRAMMAR_MUTATOR_LIB)
	$(CXX) $(C_FLAGS) $< -o $@ -Wl,-rpath,$(realpath ./) $(GRAMMAR_MUTATOR_LIB)

$(PARSER_OBJS): %.o: %.c
	$(CC) $(C_DEFINES) -I../include $(C_FLAGS) $(DEP_FLAGS) -pthread -o $@ -c $<

$(GRAMMAR_PARSER_PROM): $(PARSER_OBJS) $(GRAMMAR_MUTATOR_LIB)
	$(CXX) $(C_FLAGS) -pthread $(PARSER_OBJS) -o $@ -Wl,-rpath,$(realpath ./) $(GRAMMAR_MUTATOR_LIB)

benchmark/benchmark.o: benchmark/benchmark.c
	$(CC) $(C_DEFINES) -I../include -I. -I../third_party/rxi_map $(C_FLAGS) $(DEP_FLAGS) -o $@ -c $<

benchmark/mutants.o: benchmark/mutants.c
	$(CC) $(C_DEFINES) -I../include $(C_FLAGS) $(DEP_FLAGS) -o $@ -c $<

$(BENCH_PROM): $(BENCHMARK_OBJS) $(GRAMMAR_MUTATOR_LIB)
	$(CXX) $(C_FLAGS) $(BENCHMARK_OBJS) -o $@ -Wl,-rpath,$(realpath ./) $(GRAMMAR_MUTATOR_LIB)

-include $(OBJS:.o=.d)

.PHONY: clean
clean:
	@rm -f $(OBJS) $(OBJS:.o=.d)
	@rm -f libgrammarmutator-*.so grammar_generator-* benchmark/benchmark-*
