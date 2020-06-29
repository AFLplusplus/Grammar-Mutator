all: test_json_custom_mutator test_parsing_tree

# Test case 1:
# run a custom mutator library, which generates test cases for a specific
# grammar
test_custom_mutator: test_custom_mutator.c
	@$(MAKE) -C ../src libjsonmutator.so
	@$(CC) $(CFLAGS) -g -I../include $^ -ldl -o $@

test_json_custom_mutator: test_custom_mutator
	@./test_custom_mutator $(abspath ../src/libjsonmutator.so) > /dev/null \
		&& echo "test_json_custom_mutator: success!"
	@echo "Valgrind ..."
	@valgrind --leak-check=full --show-leak-kinds=all \
		./test_custom_mutator $(abspath ../src/libjsonmutator.so)

# Test case 2:
# test the parsing tree APIs
test_parsing_tree: test_parsing_tree.c ../src/parsing_tree.c
	@$(CC) $(CFLAGS) -g -I../include $^ -o $@
	@./$@ > /dev/null && echo "test_parsing_tree: success!"
	@echo "Valgrind ..."
	@valgrind --leak-check=full --show-leak-kinds=all ./$@

clean:
	rm -f test_custom_mutator
	rm -f test_parsing_tree
