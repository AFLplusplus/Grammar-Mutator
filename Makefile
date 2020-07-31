export ENABLE_DEBUG
export ENABLE_TESTING

ifndef GRAMMAR_FILE
override GRAMMAR_FILE = grammars/json_grammar.json
endif

ifneq "$(shell cat .grammar 2> /dev/null)" "$(realpath $(GRAMMAR_FILE))"
$(shell echo $(realpath $(GRAMMAR_FILE)) > .grammar)
endif

export GRAMMAR_FILE

PYTHON = python3

GEN_FILES = .grammar src/f1_c_fuzz.c include/f1_c_fuzz.h

.PHONY: all
all: build
ifdef ENABLE_TESTING
all: build_test
endif

src/f1_c_fuzz.c: grammars/f1_c_gen.py .grammar
include/f1_c_fuzz.h: grammars/f1_c_gen.py .grammar
	@echo "Grammar: $(shell cat .grammar)"
	@$(PYTHON) $< $(shell cat .grammar) $(CURDIR)

.PHONY: build
build: src/f1_c_fuzz.c include/f1_c_fuzz.h
	@$(MAKE) -C src all

.PHONY: build_test
build_test:
	@$(MAKE) -C tests build

.PHONY: test
test:
	@$(MAKE) -C tests all

.PHONY: test_memcheck
test_memcheck:
	@$(MAKE) -C tests memcheck

.PHONY: clean
clean:
	@rm -f $(GEN_FILES)
	@$(MAKE) -C src clean
	@$(MAKE) -C tests clean
