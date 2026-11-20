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
ifeq "$(filter $(MAKECMDGOALS),deps)" "deps"
  override BUILD = no
endif
ifeq "$(filter $(MAKECMDGOALS),antlr-jar)" "antlr-jar"
  override BUILD = no
endif
ifeq "$(filter $(MAKECMDGOALS),test-python)" "test-python"
  override BUILD = no
endif
ifeq "$(filter $(MAKECMDGOALS),format-deps)" "format-deps"
  override BUILD = no
endif

# ANTLR version. The ANTLR jar must have the same version as the C++ runtime
# in third_party/antlr4-cpp-runtime.
export ANTLR_VERSION = 4.13.2
# The jar that "make antlr-jar" downloads into this folder
ANTLR_JAR = antlr-$(ANTLR_VERSION)-complete.jar

ifeq ($(BUILD),yes)

  ifeq "$(ANTLR_JAR_LOCATION)" ""
    ANTLR_JAR_LOCATION = $(shell ls antlr-$(ANTLR_VERSION)-complete.jar 2>/dev/null)
  endif

  ifeq "$(ANTLR_JAR_LOCATION)" ""
    ANTLR_JAR_LOCATION = $(shell ls /usr/local/lib/antlr-$(ANTLR_VERSION)-complete.jar 2>/dev/null)
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
  # Check the version of the ANTLR jar
  ANTLR_JAR_VERSION := $(shell java -jar $(ANTLR_JAR_LOCATION) 2>/dev/null | sed -n 's/.*Version \([0-9.]*\).*/\1/p')
  ifneq "$(ANTLR_JAR_VERSION)" "$(ANTLR_VERSION)"
    $(error The ANTLR jar has version '$(ANTLR_JAR_VERSION)', but the build needs $(ANTLR_VERSION): $(ANTLR_JAR_LOCATION))
  endif

  ifndef GRAMMAR_FILE
    $(error Missing the grammar file path. Please specify it's path using: make GRAMMAR_FILE=<path>)
  else
    TEMP := $(abspath $(GRAMMAR_FILE))
    override GRAMMAR_FILE := $(TEMP)
  endif
  # Check whether each grammar file exists
  $(foreach f,$(GRAMMAR_FILE),$(if $(wildcard $(f)),,$(error The grammar file does not exist: $(f))))
  # A JSON grammar is one .json file. An ANTLR grammar is one or more .g4
  # files, for example a lexer grammar and a parser grammar.
  G4_FILES := $(filter %.g4,$(GRAMMAR_FILE))
  ifneq "$(G4_FILES)" ""
    ifneq "$(G4_FILES)" "$(GRAMMAR_FILE)"
      $(error Do not mix .g4 files with other grammar files: $(GRAMMAR_FILE))
    endif
    # g4_to_json.py writes this JSON grammar and the parse map from the .g4
    # files
    GRAMMAR_JSON := $(CURDIR)/grammars/g4_grammar.json
    PARSE_MAP := $(CURDIR)/grammars/g4_parse_map.json
    PARSER_DIR := $(CURDIR)/grammars/g4_python
    # The folder with the base classes and the rewrite script of a grammar
    # with action code, for example grammars-v4 javascript/javascript/Python3
    ifdef G4_SUPPORT_DIR
      override G4_SUPPORT_DIR := $(abspath $(G4_SUPPORT_DIR))
      ifeq (,$(wildcard $(G4_SUPPORT_DIR)/.))
        $(error G4_SUPPORT_DIR is not a folder: $(G4_SUPPORT_DIR))
      endif
    endif
  else
    ifneq "$(words $(GRAMMAR_FILE))" "1"
      $(error Give one JSON grammar file: $(GRAMMAR_FILE))
    endif
    ifdef GRAMMAR_START
      $(error GRAMMAR_START works only with .g4 grammar files)
    endif
    ifdef G4_SUPPORT_DIR
      $(error G4_SUPPORT_DIR works only with .g4 grammar files)
    endif
    GRAMMAR_JSON := $(GRAMMAR_FILE)
  endif
  # .grammar records the grammar files, the start rule, and the support
  # folder. A change in them makes the build generate the code again.
  GRAMMAR_STAMP := $(strip $(GRAMMAR_FILE) $(GRAMMAR_START) $(G4_SUPPORT_DIR))
  ifneq "$(shell cat .grammar 2> /dev/null)" "$(GRAMMAR_STAMP)"
    # Create or update .grammar
    $(info Create or update .grammar)
    $(shell echo $(GRAMMAR_STAMP) > .grammar)
  endif
  ifndef GRAMMAR_FILENAME
    ifneq "$(G4_FILES)" ""
      # For example, JavaScriptLexer.g4 JavaScriptParser.g4 give "javascript"
      GRAMMAR_FILENAME := $(shell basename $(lastword $(G4_FILES)) .g4 | sed -E 's/(Parser|Lexer)$$//' | tr A-Z a-z)
    else
      GRAMMAR_FILENAME := $(shell basename $(GRAMMAR_FILE) | sed 's/[_.-].*//' | tr A-Z a-z)
    endif
    $(info Selected grammar name: $(GRAMMAR_FILENAME) (from $(GRAMMAR_FILE)))
  else
    $(info Selected grammar name: $(GRAMMAR_FILENAME))
  endif

endif

PYTHON = python3
# The stamp of the Python tools in .venv (see "make deps")
VENV_STAMP = .venv/.requirements.stamp
C_FLAGS_OPT =
CXX_FLAGS_OPT =
export C_FLAGS_OPT
export CXX_FLAGS_OPT

GEN_FILES = \
	.grammar \
	grammars/Grammar.g4 \
	grammars/g4_grammar.json grammars/g4_parse_map.json grammars/g4_python \
	grammars/antlr4/generated \
	src/f1_c_fuzz.c include/f1_c_fuzz.h \
	lib/antlr4_shim/generated

.PHONY: all
all: build

# Generation
src/f1_c_fuzz.c include/f1_c_fuzz.h: grammars/f1_c_gen.py .grammar $(GRAMMAR_JSON)
	$(PYTHON) grammars/f1_c_gen.py $(GRAMMAR_JSON) $(CURDIR)

# The launcher of the parser for .g4 files. It runs grammars/g4_parse.py with
# the grammar and the Python parser. The launcher uses only its own copy of
# them in PARSER_DATA, so a build of a different grammar does not change it.
PARSER_LAUNCHER = src/grammar_parser-$(GRAMMAR_FILENAME)
PARSER_DATA = $(PARSER_LAUNCHER).d

# The Python parser for .g4 files, from the ANTLR 4 meta-grammar. A build
# finds the jar (ANTLR_JAR_LOCATION). "make test-python" does not, so it uses
# (and downloads) ANTLR_JAR.
META_PARSER = grammars/antlr4/generated/ANTLRv4Parser.py
META_ANTLR_JAR = $(or $(ANTLR_JAR_LOCATION),$(ANTLR_JAR))

$(META_PARSER): grammars/antlr4/ANTLRv4Lexer.g4 grammars/antlr4/ANTLRv4Parser.g4 $(META_ANTLR_JAR)
	java -jar $(META_ANTLR_JAR) -Dlanguage=Python3 \
	     -Xexact-output-dir -o grammars/antlr4/generated $(filter %.g4,$^)

ifneq "$(G4_FILES)" ""
$(GRAMMAR_JSON): .grammar grammars/g4_to_json.py $(META_PARSER) $(VENV_STAMP) $(G4_FILES)
	.venv/bin/python grammars/g4_to_json.py $(if $(GRAMMAR_START),--start $(GRAMMAR_START)) \
	     --parse-map $(PARSE_MAP) -o $@ $(G4_FILES)

# g4_to_json.py writes the parse map with the JSON grammar
$(PARSE_MAP): $(GRAMMAR_JSON)
	@test -f $@ || { echo "Missing $@. Run make clean."; exit 1; }

# The Python parser: the .g4 files (changed by transformGrammar.py, if the
# support folder has it), the generated lexer and parser, and the base classes
# of the support folder. Without G4_SUPPORT_DIR, a grammar with action code
# gives a clear message when grammar_parser-<name> runs.
PARSER_STAMP = $(PARSER_DIR)/.built
$(PARSER_STAMP): .grammar $(G4_FILES) $(if $(G4_SUPPORT_DIR),$(wildcard $(G4_SUPPORT_DIR)/*)) $(VENV_STAMP)
	rm -rf $(PARSER_DIR)
	mkdir -p $(PARSER_DIR)
	cp $(G4_FILES) $(PARSER_DIR)/
	$(if $(G4_SUPPORT_DIR),if [ -f $(G4_SUPPORT_DIR)/transformGrammar.py ]; then \
	     (cd $(PARSER_DIR) && $(CURDIR)/.venv/bin/python $(G4_SUPPORT_DIR)/transformGrammar.py); fi)
	$(if $(G4_SUPPORT_DIR),for f in $(G4_SUPPORT_DIR)/*.py; do \
	     [ "$$(basename $$f)" = transformGrammar.py ] || cp $$f $(PARSER_DIR)/; done)
	java -jar $(ANTLR_JAR_LOCATION) -Dlanguage=Python3 -Xexact-output-dir \
	     -o $(PARSER_DIR) $(PARSER_DIR)/*.g4
	@touch $@

$(PARSER_LAUNCHER): grammars/grammar_parser.in .grammar $(GRAMMAR_JSON) $(PARSE_MAP) $(PARSER_STAMP)
	rm -rf $(PARSER_DATA)
	mkdir -p $(PARSER_DATA)
	cp $(GRAMMAR_JSON) $(PARSER_DATA)/grammar.json
	cp $(PARSE_MAP) $(PARSER_DATA)/parse_map.json
	cp -R $(PARSER_DIR) $(PARSER_DATA)/python
	sed -e 's|@VENV_PYTHON@|$(CURDIR)/.venv/bin/python|' \
	    -e 's|@G4_PARSE@|$(CURDIR)/grammars/g4_parse.py|' \
	    -e 's|@PARSER_DATA@|$(CURDIR)/$(PARSER_DATA)|g' \
	    grammars/grammar_parser.in > $@
	chmod +x $@

# The shim gets a parser that rejects all input. grammar_parser-<name> parses
# the test cases.
PARSER_G4 = grammars/g4_placeholder/Grammar.g4
else
grammars/Grammar.g4: grammars/f1_g4_translate.py .grammar $(GRAMMAR_JSON)
	$(PYTHON) grammars/f1_g4_translate.py $(GRAMMAR_JSON) ./grammars

PARSER_G4 = grammars/Grammar.g4
endif

lib/antlr4_shim/generated: $(PARSER_G4) .grammar
	@$(MAKE) -C lib clean
	java -jar $(ANTLR_JAR_LOCATION) \
	     -Dlanguage=Cpp -DcontextSuperClass=antlr4::RuleContextWithAltNum \
	     -o lib/antlr4_shim/generated \
	     $(abspath $(PARSER_G4))

.PHONY: build
build: src/f1_c_fuzz.c include/f1_c_fuzz.h third_party build_lib
	@$(MAKE) -C src all GRAMMAR_FILE="$(GRAMMAR_FILE)" GRAMMAR_FILENAME=$(GRAMMAR_FILENAME)
	@ln -sf src/grammar_generator-$(GRAMMAR_FILENAME) grammar_generator-$(GRAMMAR_FILENAME)
	@ln -sf src/libgrammarmutator-$(GRAMMAR_FILENAME).so libgrammarmutator-$(GRAMMAR_FILENAME).so
ifneq "$(G4_FILES)" ""
	@$(MAKE) $(PARSER_LAUNCHER)
	@ln -sf $(PARSER_LAUNCHER) grammar_parser-$(GRAMMAR_FILENAME)
endif

.PHONY: build_lib
build_lib: lib/antlr4_shim/generated src/f1_c_fuzz.c include/f1_c_fuzz.h third_party
	@$(MAKE) -C lib all

ifdef ENABLE_TESTING
all: build_test

.PHONY: build_test
build_test: build third_party
	@$(MAKE) -C tests build GRAMMAR_FILE="$(GRAMMAR_FILE)" GRAMMAR_FILENAME=$(GRAMMAR_FILENAME)
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

# Unit tests for the Python tools (grammars/g4_*.py). Some tests run the jar.
.PHONY: test-python
test-python: $(VENV_STAMP) $(META_PARSER) $(META_ANTLR_JAR)
	ANTLR_JAR=$(abspath $(META_ANTLR_JAR)) .venv/bin/python -m unittest discover -s tests/python

# Python tools in .venv. requirements.txt pins their versions. The stamp file
# installs them again when requirements.txt changes.
$(VENV_STAMP): requirements.txt
	$(PYTHON) -m venv .venv
	.venv/bin/pip install --quiet -r requirements.txt
	@touch $@

.PHONY: deps
deps: $(VENV_STAMP)

.PHONY: format-deps
format-deps: deps

# The clang-format major version from requirements.txt. Other major versions
# give different output.
CLANG_FORMAT_VERSION := $(shell sed -n 's/^clang-format==\([0-9][0-9]*\)\..*/\1/p' requirements.txt)

# Download the ANTLR jar that matches ANTLR_VERSION into this folder. Build
# scripts (for example AFL++'s build_grammar_mutator.sh) can use this target, so
# that they do not need to know the ANTLR version.
.PHONY: antlr-jar
antlr-jar: $(ANTLR_JAR)

$(ANTLR_JAR):
	wget -q -O $@.tmp https://www.antlr.org/download/$@ && mv $@.tmp $@

.PHONY: code-format
code-format: export LLVM_VERSION = $(CLANG_FORMAT_VERSION)
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
	@rm -rf src/grammar_parser-*.d
	@rm -f grammar_generator-* libgrammarmutator-*.so grammar_parser-* src/grammar_parser-*

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
	@echo "test-python: runs the unit tests of the Python tools"
	@echo "clean: cleans everything compiled"
	@echo "antlr-jar: downloads the ANTLR $(ANTLR_VERSION) jar into this folder"
	@echo "code-format: format the code with clang-format $(CLANG_FORMAT_VERSION)"
	@echo "deps: install the Python tools (requirements.txt) into .venv with pip"
	@echo "format-deps: same as deps"
	@echo "help: shows help information"
	@echo "=========================================="
	@echo
	@echo "Known build environment options:"
	@echo "=========================================="
	@echo "ENABLE_TESTING - compiles test cases"
	@echo "ENABLE_DEBUG - compiles with '-g' option for debug purposes"
	@echo "GRAMMAR_FILE - the path to the input grammar file: one .json file, or one or"
	@echo "               more ANTLR 4 .g4 files (for example \"Lexer.g4 Parser.g4\")"
	@echo "GRAMMAR_START - the start rule of a .g4 grammar (default: the first parser rule)"
	@echo "G4_SUPPORT_DIR - for a .g4 grammar with action code: the grammars-v4 folder of the"
	@echo "                 target language (for example javascript/javascript/Python3)"
	@echo "GRAMMAR_FILENAME - name that will be used in the naming of the generated grammar"
	@echo "                   files, e.g. \"ruby\" => ./grammar_generator-ruby"
	@echo "ANTLR_JAR_LOCATION - the path to ANTLR4 jar file"
	@echo "=========================================="
	@echo "e.g.: make ENABLE_TESTING=1 GRAMMAR_FILE=./grammars/ruby.json \\"
	@echo "           ANTLR_JAR_LOCATION=./antlr-$(ANTLR_VERSION)-complete.jar"
