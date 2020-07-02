# Test case 1:
# run a custom mutator library, which generates test cases for a specific
# grammar
add_executable(test_custom_mutator
  test_custom_mutator.c
)

target_link_libraries(test_custom_mutator PRIVATE dl)

# Test case 2:
# test the parsing tree APIs
add_executable(test_parsing_tree
  test_parsing_tree.c
  ${CMAKE_SOURCE_DIR}/src/parsing_tree.c
)
