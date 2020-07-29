export ENABLE_DEBUG
export ENABLE_TESTING

ifndef GRAMMAR_FILE
$(info Use default JSON grammar file)
GRAMMAR_FILE = grammars/json_grammar.json
else
$(info Grammar: $(realpath $(GRAMMAR_FILE)))
endif
export GRAMMAR_FILE

PYTHON = python3

.PHONY: all
all: build

ifdef ENABLE_TESTING
all: build_test
endif

src/f1_c_fuzz.c: grammars/f1_c_gen.py
include/f1_c_fuzz.h: grammars/f1_c_gen.py
	@$(PYTHON) $< $(realpath $(GRAMMAR_FILE)) $(CURDIR)

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
	@$(MAKE) -C src clean
	@$(MAKE) -C tests clean
