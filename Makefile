export ENABLE_DEBUG

.PHONY: all
all: build

.PHONY: build
build:
	$(MAKE) -C src all

.PHONY: test
test:
	$(MAKE) -C tests all

.PHONY: test_memcheck
test_memcheck:
	$(MAKE) -C tests memcheck

.PHONY: clean
clean:
	$(MAKE) -C src clean
	$(MAKE) -C tests clean
