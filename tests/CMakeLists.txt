#
# american fuzzy lop++ - grammar mutator
# --------------------------------------
#
# Written by Shengtuo Hu
#
# Copyright 2020 AFLplusplus Project. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# A grammar-based custom mutator written for GSoC '20.
#

# Download and unpack googletest at configure time
configure_file(
  googletest-download/CMakeLists.txt.in
  googletest-download/CMakeLists.txt)
execute_process(
  COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
if (result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif ()
execute_process(
  COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
if (result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif ()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(
  ${CMAKE_CURRENT_BINARY_DIR}/googletest-src
  ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
  EXCLUDE_FROM_ALL)

include_directories(${CMAKE_SOURCE_DIR}/include)

# Test suite 1:
# run a custom mutator library, which generates test cases for a specific
# grammar
add_executable(test_custom_mutator test_custom_mutator.cpp)
target_link_libraries(test_custom_mutator
  PRIVATE gtest_main
  PRIVATE grammarmutator)
add_test(
  NAME test_custom_mutator
  COMMAND test_custom_mutator)

# Test suite 2:
# test the tree APIs
add_executable(test_tree test_tree.cpp)
target_link_libraries(test_tree
  PRIVATE gtest_main
  PRIVATE grammarmutator)
add_test(
  NAME test_tree
  COMMAND test_tree)
if ("${GRAMMAR_FILE}" STREQUAL "${CMAKE_SOURCE_DIR}/grammars/ruby_grammar.json")
  message(STATUS "Enable testing on parsing 'testcases/array.rb'")
  target_compile_definitions(test_tree
    PRIVATE ENABLE_PARSING_ARRAY_RB
    PRIVATE ARRAY_RB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/testcases/array.rb")
endif ()

# Test suite 3:
# test the tree mutation APIs
add_executable(test_tree_mutation test_tree_mutation.cpp)
target_link_libraries(test_tree_mutation
  PRIVATE gtest_main
  PRIVATE grammarmutator
  PRIVATE rxi_map)
target_include_directories(test_tree_mutation
  PRIVATE ${CMAKE_SOURCE_DIR}/third_party/rxi_map)
add_test(
  NAME test_tree_mutation
  COMMAND test_tree_mutation)

# Test suite 4:
# test the doubly linked list
add_executable(test_list test_list.cpp)
target_link_libraries(test_list
  PRIVATE gtest_main
  PRIVATE grammarmutator)
add_test(
  NAME test_list
  COMMAND test_list)

# Test suite 5:
# test the chunk store
add_executable(test_chunk_store test_chunk_store.cpp)
target_link_libraries(test_chunk_store
  PRIVATE gtest_main
  PRIVATE grammarmutator
  PRIVATE rxi_map)
target_include_directories(test_chunk_store
  PRIVATE ${CMAKE_SOURCE_DIR}/third_party/rxi_map)
add_test(
  NAME test_chunk_store
  COMMAND test_chunk_store)

# Test suite 6:
# test the tree trimming APIs
add_executable(test_tree_trimming test_tree_trimming.cpp)
target_link_libraries(test_tree_trimming
  PRIVATE gtest_main
  PRIVATE grammarmutator)
add_test(
  NAME test_tree_trimming
  COMMAND test_tree_trimming)

# Test suite 7:
# test the third-party C map library
add_executable(test_rxi_map test_rxi_map.cpp)
target_link_libraries(test_rxi_map
  PRIVATE gtest_main
  PRIVATE grammarmutator
  PRIVATE rxi_map)
target_include_directories(test_rxi_map
  PRIVATE ${CMAKE_SOURCE_DIR}/third_party/rxi_map)
add_test(
  NAME test_rxi_map
  COMMAND test_rxi_map)
