AFLPP_ROOT ?= $(abspath ../../AFLplusplus)

all: test_json_custom_mutator test_parsing_tree

# Test case 1:
# run a custom mutator library, which generates test cases for a specific
# grammar
test_custom_mutator: test_custom_mutator.c
	@$(MAKE) -C ../src libjsonmutator.so
	@$(CC) $(CFLAGS) -g -I"$(AFLPP_ROOT)/include" $^ -ldl -o $@

test_json_custom_mutator: test_custom_mutator
	@./test_custom_mutator $(abspath ../src/libjsonmutator.so) \
		&& echo "test_json_custom_mutator: success!"

# Test case 2:
# test the parsing tree APIs
test_parsing_tree: test_parsing_tree.c ../src/parsing_tree.c
	@$(CC) $(CFLAGS) -g -I"$(AFLPP_ROOT)/include" -I../include $^ -o $@
	@./$@ && echo "test_parsing_tree: success!"

clean:
	rm -f test_custom_mutator
	rm -f test_parsing_tree
