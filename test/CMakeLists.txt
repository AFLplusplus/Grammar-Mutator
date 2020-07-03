# Download and unpack googletest at configure time
configure_file(
  googletest-download/CMakeLists.txt.in
  googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(
  ${CMAKE_CURRENT_BINARY_DIR}/googletest-src
  ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
  EXCLUDE_FROM_ALL)


# Test case 1:
# run a custom mutator library, which generates test cases for a specific
# grammar
add_executable(test_custom_mutator
  test_custom_mutator.cpp)
add_dependencies(test_custom_mutator jsonmutator)
target_link_libraries(test_custom_mutator
  PRIVATE dl
  PRIVATE gtest_main)
add_test(
  NAME test_json_custom_mutator
  COMMAND test_custom_mutator ${CMAKE_BINARY_DIR}/src/libjsonmutator.so)

# Test case 2:
# test the tree APIs
add_executable(test_tree
  test_tree.cpp
  ${CMAKE_SOURCE_DIR}/src/tree.c)
target_link_libraries(test_tree PRIVATE gtest_main)
add_test(
  NAME test_tree
  COMMAND test_tree)

# Test case 3:
# test the tree mutation APIs
add_executable(test_tree_mutation
  test_tree_mutation.cpp
  ${CMAKE_SOURCE_DIR}/src/tree.c
  ${CMAKE_SOURCE_DIR}/src/tree_mutation.c
  ${CMAKE_SOURCE_DIR}/src/json_c_fuzz.c)
target_link_libraries(test_tree_mutation PRIVATE gtest_main)
add_test(
  NAME test_tree_mutation
  COMMAND test_tree_mutation)
