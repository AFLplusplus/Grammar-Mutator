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

# Grammar mutator
add_library(grammarmutator SHARED
  chunk_store.c
  list.c
  tree.c
  tree_mutation.c
  tree_trimming.c
  ${CMAKE_BINARY_DIR}/f1/src/f1_c_fuzz.c
  grammar_mutator.c
  utils.c)
target_link_libraries(grammarmutator
  PRIVATE rxi_map
  PRIVATE xxhash
  PRIVATE antlr4_shim)
target_include_directories(grammarmutator
  PUBLIC ${CMAKE_SOURCE_DIR}/include
  PUBLIC ${CMAKE_BINARY_DIR}/f1/include  # Generated headers
  PRIVATE ${CMAKE_SOURCE_DIR}/third_party/rxi_map
  PRIVATE ${CMAKE_SOURCE_DIR}/third_party/Cyan4973_xxHash)
set_target_properties(grammarmutator
  PROPERTIES OUTPUT_NAME "grammarmutator-${GRAMMAR_FILENAME}")

# Grammar generator
add_executable(grammar_generator
  grammar_generator.c)
target_link_libraries(grammar_generator
  PRIVATE grammarmutator)
target_include_directories(grammarmutator
  PUBLIC ${CMAKE_SOURCE_DIR}/include
  PUBLIC ${CMAKE_BINARY_DIR}/f1/include)
set_target_properties(grammar_generator
  PROPERTIES OUTPUT_NAME "grammar_generator-${GRAMMAR_FILENAME}")

add_subdirectory(benchmark)
