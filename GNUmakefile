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

BUILD = yes
ifeq "$(filter $(MAKECMDGOALS),test)" "test"
  override BUILD = no
endif
ifeq "$(filter $(MAKECMDGOALS),test_memcheck)" "test_memcheck"
  override BUILD = no
endif
ifeq "$(filter $(MAKECMDGOALS),clean)" "clean"
  override BUILD = no
endif
ifeq "$(filter $(MAKECMDGOALS),help)" "help"
  override BUILD = no
endif
ifeq "$(filter $(MAKECMDGOALS),code-format)" "code-format"
  override BUILD = no
endif

ifeq ($(BUILD),yes)

  ifeq "$(ANTLR_JAR_LOCATION)" ""
    ANTLR_JAR_LOCATION = $(shell ls antlr-4.8-complete.jar 2>/dev/null)
  endif

  ifeq "$(ANTLR_JAR_LOCATION)" ""
    ANTLR_JAR_LOCATION = $(shell ls /usr/local/lib/antlr-4.8-complete.jar 2>/dev/null)
  endif

  ifeq "$(ANTLR_JAR_LOCATION)" ""
    $(error Missing antlr4.jar path. Please specify it's path using: make ANTLR_JAR_LOCATION=<path>)
  else
    TEMP := $(abspath $(ANTLR_JAR_LOCATION))
    override ANTLR_JAR_LOCATION := $(TEMP)
  endif
  # Check whether ANTLR jar exists
  ifeq (,$(wildcard $(ANTLR_JAR_LOCATION)))
    $(error Unable to find antlr4.jar in $(ANTLR_JAR_LOCATION))
  endif
  ANTLR_NAME := $(basename $(shell basename $(ANTLR_JAR_LOCATION)))
  $(info Found $(ANTLR_NAME): $(ANTLR_JAR_LOCATION))

  ifndef GRAMMAR_FILE
    $(error Missing the grammar file path. Please specify it's path using: make GRAMMAR_FILE=<path>)
  else
    TEMP := $(abspath $(GRAMMAR_FILE))
    override GRAMMAR_FILE := $(TEMP)
  endif
  # Check whether the grammar file exists
  ifeq (,$(wildcard $(GRAMMAR_FILE)))
    $(error The grammar file does not exist: $(GRAMMAR_FILE))
  endif
  ifneq "$(shell cat .grammar 2> /dev/null)" "$(abspath $(GRAMMAR_FILE))"
    # Create or update .grammar
    $(info Create or update .grammar)
    $(shell echo $(abspath $(GRAMMAR_FILE)) > .grammar)
  endif
  ifndef GRAMMAR_FILENAME
    GRAMMAR_FILENAME := $(shell basename $(GRAMMAR_FILE) | sed 's/[_.-].*//' | tr A-Z a-z)
    $(info Selected grammar name: $(GRAMMAR_FILENAME) (from $(GRAMMAR_FILE)))
  else
    $(info Selected grammar name: $(GRAMMAR_FILENAME))
  endif

endif

PYTHON = python3
C_FLAGS_OPT = -Wall -Wextra -Werror
CXX_FLAGS_OPT = -Wall -Wextra -Werror
export C_FLAGS_OPT
export CXX_FLAGS_OPT

GEN_FILES = \
	.grammar \
	grammars/Grammar.g4 \
	src/f1_c_fuzz.c include/f1_c_fuzz.h \
	lib/antlr4_shim/generated

.PHONY: all
all: build

# Generation
src/f1_c_fuzz.c include/f1_c_fuzz.h: grammars/f1_c_gen.py .grammar
	$(PYTHON) grammars/f1_c_gen.py $(shell cat .grammar) $(CURDIR)

lib/antlr4_shim/generated: grammars/f1_g4_translate.py .grammar
	$(PYTHON) grammars/f1_g4_translate.py $(shell cat .grammar) ./grammars
	@$(MAKE) -C lib clean
	java -jar $(ANTLR_JAR_LOCATION) \
	     -Dlanguage=Cpp -DcontextSuperClass=antlr4::RuleContextWithAltNum \
	     -o lib/antlr4_shim/generated \
	     $(abspath grammars/Grammar.g4)

.PHONY: build
build: src/f1_c_fuzz.c include/f1_c_fuzz.h third_party build_lib
	@$(MAKE) -C src all GRAMMAR_FILE=$(GRAMMAR_FILE) GRAMMAR_FILENAME=$(GRAMMAR_FILENAME)
	@ln -sf src/grammar_generator-$(GRAMMAR_FILENAME) grammar_generator-$(GRAMMAR_FILENAME)
	@ln -sf src/libgrammarmutator-$(GRAMMAR_FILENAME).so libgrammarmutator-$(GRAMMAR_FILENAME).so

.PHONY: build_lib
build_lib: lib/antlr4_shim/generated src/f1_c_fuzz.c include/f1_c_fuzz.h third_party
	@$(MAKE) -C lib all

ifdef ENABLE_TESTING
all: build_test

.PHONY: build_test
build_test: build third_party
	@$(MAKE) -C tests build GRAMMAR_FILE=$(GRAMMAR_FILE) GRAMMAR_FILENAME=$(GRAMMAR_FILENAME)
endif

.PHONY: third_party
third_party:
	@$(MAKE) -C third_party

.PHONY: test
test:
	@$(MAKE) -C tests all GRAMMAR_FILENAME=$(GRAMMAR_FILENAME)

.PHONY: test_memcheck
test_memcheck:
	@$(MAKE) -C tests memcheck

.PHONY: code-format
code-format:
	./.custom-format.py -i src/*.c
	./.custom-format.py -i tests/*.cpp
	./.custom-format.py -i src/*.h
	./.custom-format.py -i tests/*.h
	./.custom-format.py -i include/*.h

.PHONY: clean
clean:
	@$(MAKE) -C lib $@
	@$(MAKE) -C src $@
	@$(MAKE) -C tests $@
	@$(MAKE) -C third_party $@
	@rm -rf $(GEN_FILES)
	@rm -rf grammars/__pycache__
	@rm -f grammar_generator-* libgrammarmutator-*.so

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
	@echo "code-format: format the code with a clang-format config (llvm 10)"
	@echo "help: shows help information"
	@echo "=========================================="
	@echo
	@echo "Known build environment options:"
	@echo "=========================================="
	@echo "ENABLE_TESTING - compiles test cases"
	@echo "ENABLE_DEBUG - compiles with '-g' option for debug purposes"
	@echo "GRAMMAR_FILE - the path to the input grammar file"
	@echo "GRAMMAR_FILENAME - name that will be used in the naming of the generated grammar"
	@echo "                   files, e.g. \"ruby\" => ./grammar_generator-ruby"
	@echo "ANTLR_JAR_LOCATION - the path to ANTLR4 jar file"
	@echo "=========================================="
	@echo "e.g.: make ENABLE_TESTING=1 GRAMMAR_FILE=./grammars/ruby.json \\"
	@echo "           ANTLR_JAR_LOCATION=./antlr-4.8-complete.jar"
