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
$(info Use default JSON grammar file)
override GRAMMAR_FILE = $(realpath grammars/json_grammar.json)
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
$(error Missing antlr4.jar location. Please specify it's path using: make ANTLR_JAR_LOCATION=<path>)
else
TEMP := $(realpath $(ANTLR_JAR_LOCATION))
override ANTLR_JAR_LOCATION := $(TEMP)
endif

PYTHON = python3

GEN_FILES = .grammar src/f1_c_fuzz.c include/f1_c_fuzz.h grammars/Grammar.g4

.PHONY: all
all: build

src/f1_c_fuzz.c include/f1_c_fuzz.h: grammars/f1_c_gen.py .grammar
	@echo "Grammar: $(shell cat .grammar)"
	$(PYTHON) $< $(shell cat .grammar) $(CURDIR)

.PHONY: build
build: src/f1_c_fuzz.c include/f1_c_fuzz.h third_party build_lib
	@$(MAKE) -C src all

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

.PHONY: help
help:
	@echo "HELP --- the following make targets exist:"
	@echo "=========================================="
	@echo "all: compiles everything"
	@echo "build: compiles the grammar mutator library"
	@echo "build_test: compiles all test cases (if ENABLE_TESTING=1)"
	@echo "test: runs the testing framework"
	@echo "test_memcheck: runs Valgrind with all test cases to pinpoint memory leaks"
	@echo "clean: cleans everything compiled"
	@echo "help: shows help information"
	@echo "=========================================="
	@echo
	@echo "Known build environment options:"
	@echo "=========================================="
	@echo "ENABLE_TESTING - compiles test cases"
	@echo "ENABLE_DEBUG - compiles with '-g' option for debug purposes"
	@echo "GRAMMAR_FILE - the path to the input grammar file (Default: grammars/json_grammar.json)"
	@echo "ANTLR_JAR_LOCATION" - the path to ANTLR4 jar file
	@echo "=========================================="
	@echo "e.g.: make ENABLE_TESTING=1 GRAMMAR_FILE=./grammars/json_grammar.json"
