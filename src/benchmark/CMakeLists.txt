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

# A program to benchmark the grammar mutator
add_executable(benchmark
  benchmark.c)
target_link_libraries(benchmark
  PRIVATE grammarmutator
  PRIVATE -lm)
target_include_directories(benchmark
  PUBLIC ${CMAKE_SOURCE_DIR}/include
  PUBLIC ${CMAKE_BINARY_DIR}/f1/include)
set_target_properties(benchmark
  PROPERTIES OUTPUT_NAME "benchmark-${GRAMMAR_FILENAME}")
