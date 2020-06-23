all: src test
clean_all: clean clean_test

src:
	$(MAKE) -C $@ all

clean:
	$(MAKE) -C src clean

# Test
test:
	$(MAKE) -C $@ all

clean_test:
	$(MAKE) -C test clean

.PHONY: all src test clean_all clean clean_test
