export ENABLE_DEBUG
export ENABLE_TESTING

.PHONY: all
all: build

ifdef ENABLE_TESTING
all: build_test
endif

.PHONY: build
build:
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
