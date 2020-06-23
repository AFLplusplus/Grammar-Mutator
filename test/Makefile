AFLPP_ROOT ?= $(abspath ../../AFLplusplus)

all: test_json_custom_mutator

test_custom_mutator: test_custom_mutator.c
	$(MAKE) -C ../src libjsonmutator.so
	$(CC) $(CFLAGS) -g -I "$(AFLPP_ROOT)/include" $^ -ldl -o $@

test_json_custom_mutator: test_custom_mutator
	./test_custom_mutator $(abspath ../src/libjsonmutator.so)

clean:
	rm -f test_custom_mutator
