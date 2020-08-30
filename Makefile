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

export ENABLE_DEBUG
export ENABLE_TESTING

ifeq "$(filter $(MAKECMDGOALS),test)" "test"
override GRAMMAR_FILE = $(shell cat .grammar 2> /dev/null)
override ANTLR_JAR_LOCATION = ""
endif
ifeq "$(filter $(MAKECMDGOALS),test_memcheck)" "test_memcheck"
override GRAMMAR_FILE = $(shell cat .grammar 2> /dev/null)
override ANTLR_JAR_LOCATION = ""
endif
ifeq "$(filter $(MAKECMDGOALS),clean)" "clean"
override GRAMMAR_FILE = ""
override ANTLR_JAR_LOCATION = ""
endif
ifeq "$(filter $(MAKECMDGOALS),help)" "help"
override GRAMMAR_FILE = ""
override ANTLR_JAR_LOCATION = ""
endif

ifndef GRAMMAR_FILE
$(error Missing the grammar file path. Please specify it's path using: make GRAMMAR_FILE=<path>)
else
TEMP := $(realpath $(GRAMMAR_FILE))
override GRAMMAR_FILE := $(TEMP)
endif

ifneq "$(shell cat .grammar 2> /dev/null)" "$(realpath $(GRAMMAR_FILE))"
# Create or update .grammar
$(info Create or update .grammar)
$(shell echo $(realpath $(GRAMMAR_FILE)) > .grammar)
endif

ifndef ANTLR_JAR_LOCATION
$(error Missing antlr4.jar path. Please specify it's path using: make ANTLR_JAR_LOCATION=<path>)
else
TEMP := $(realpath $(ANTLR_JAR_LOCATION))
override ANTLR_JAR_LOCATION := $(TEMP)
endif

PYTHON = python3
C_FLAGS_OPT = -Wall -Wextra -Werror
CXX_FLAGS_OPT = -Wall -Wextra -Werror
export C_FLAGS_OPT
export CXX_FLAGS_OPT

GEN_FILES = .grammar src/f1_c_fuzz.c include/f1_c_fuzz.h grammars/Grammar.g4

.PHONY: all
all: build

src/f1_c_fuzz.c include/f1_c_fuzz.h: grammars/f1_c_gen.py .grammar
	@echo "Grammar: $(shell cat .grammar)"
	$(PYTHON) $< $(shell cat .grammar) $(CURDIR)

.PHONY: build
build: src/f1_c_fuzz.c include/f1_c_fuzz.h third_party build_lib
	@$(MAKE) -C src all
	@ln -sf src/grammar_generator grammar_generator
	@ln -sf src/libgrammarmutator.so libgrammarmutator.so

grammars/Grammar.g4: grammars/f1_g4_translate.py .grammar
	$(PYTHON) $< $(shell cat .grammar) ./grammars

.PHONY: build_lib
build_lib: grammars/Grammar.g4 src/f1_c_fuzz.c include/f1_c_fuzz.h third_party
	@$(MAKE) -C lib all ANTLR_JAR_LOCATION=$(ANTLR_JAR_LOCATION)

ifdef ENABLE_TESTING
all: build_test

.PHONY: build_test
build_test: third_party
	@$(MAKE) -C tests build GRAMMAR_FILE=$(GRAMMAR_FILE)
endif

.PHONY: third_party
third_party:
	@$(MAKE) -C third_party

.PHONY: test
test:
	@$(MAKE) -C tests all

.PHONY: test_memcheck
test_memcheck:
	@$(MAKE) -C tests memcheck

.PHONY: clean
clean:
	@$(MAKE) -C lib $@
	@$(MAKE) -C src $@
	@$(MAKE) -C tests $@
	@$(MAKE) -C third_party $@
	@rm -f $(GEN_FILES)
	@rm -rf grammars/__pycache__
	@rm -f grammar_generator libgrammarmutator.so

.PHONY: help
help:
	@echo "HELP --- the following make targets exist:"
	@echo "=========================================="
	@echo "all: compiles everything"
	@echo "build: compiles the grammar mutator library"
	@echo "build_test: compiles all test cases (if ENABLE_TESTING=1)"
	@echo "test: runs the testing framework (if ENABLE_TESTING=1)"
	@echo "test_memcheck: runs Valgrind with all test cases to pinpoint memory leaks"
	@echo "               (if ENABLE_TESTING=1 and have Valgrind)"
	@echo "clean: cleans everything compiled"
	@echo "help: shows help information"
	@echo "=========================================="
	@echo
	@echo "Known build environment options:"
	@echo "=========================================="
	@echo "ENABLE_TESTING - compiles test cases"
	@echo "ENABLE_DEBUG - compiles with '-g' option for debug purposes"
	@echo "GRAMMAR_FILE - the path to the input grammar file"
	@echo "               (Default: grammars/json_grammar.json)"
	@echo "ANTLR_JAR_LOCATION - the path to ANTLR4 jar file"
	@echo "=========================================="
	@echo "e.g.: make ENABLE_TESTING=1 GRAMMAR_FILE=./grammars/json_grammar.json \\"
	@echo "           ANTLR_JAR_LOCATION=./antlr-4.8-complete.jar"
