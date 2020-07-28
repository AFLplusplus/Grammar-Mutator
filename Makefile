export ENABLE_DEBUG

.PHONY: all
all: build

.PHONY: build
build:
	$(MAKE) -C src all

ifdef ENABLE_TESTING
.PHONY: test
test: build
	$(MAKE) -C tests all

.PHONY: test_memcheck
test_memcheck:
	$(MAKE) -C tests memcheck
endif

.PHONY: clean
clean:
	$(MAKE) -C src clean
ifdef ENABLE_TESTING
	$(MAKE) -C tests clean
endif
