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

# With ANTLR_BUILD_CPP_TESTS=OFF, the runtime build does not copy the library
# to dist/. The library stays in the build folder (see lib/antlr4_shim).
include(ExternalProject)
ExternalProject_Add(antlr4-cpp-runtime
  URL https://www.antlr.org/download/antlr4-cpp-runtime-${ANTLR_VERSION}-source.zip
  URL_HASH SHA256=${ANTLR_RUNTIME_SHA256}
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/antlr4-cpp-runtime-src"
  BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/antlr4-cpp-runtime-build"
  CONFIGURE_COMMAND ${CMAKE_COMMAND} ${CMAKE_CURRENT_BINARY_DIR}/antlr4-cpp-runtime-src
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    -DCMAKE_CXX_FLAGS=-fPIC
                    -DANTLR_BUILD_CPP_TESTS=OFF
                    -DANTLR_BUILD_SHARED=OFF
  BUILD_COMMAND ${CMAKE_COMMAND} --build . --target antlr4_static
  BUILD_BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/antlr4-cpp-runtime-build/runtime/libantlr4-runtime.a"
  INSTALL_COMMAND ""
  TEST_COMMAND "")
