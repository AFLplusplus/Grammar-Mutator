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

cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)
project(Grammar-Mutator)

# Undocumented options
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif ()

# CMake modules
# set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CMakeOptions.txt)
set(MEMORCHECK_COMMAND_OPTIONS "--leak-check=full --show-leak-kinds=all")
include(CTest)

# Check build type
if (ENABLE_DEBUG)
  message(STATUS "Enable debug output")
  add_definitions(-DDEBUG_BUILD)
endif ()
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
    "Choose the build type" FORCE)

  # Include "None" as option to disable any additional (optimization) flags,
  # relying on just CMAKE_C_FLAGS and CMAKE_CXX_FLAGS (which are empty by
  # default). These strings are presented in cmake-gui.
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "None" "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif ()

# Set -Wall -Wextra -Werror
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

# Do not disable assertions based on CMAKE_BUILD_TYPE.
foreach (_build_type "Release" "MinSizeRel" "RelWithDebInfo")
  foreach (_lang C CXX)
    string(TOUPPER "CMAKE_${_lang}_FLAGS_${_build_type}" _var)
    string(REGEX REPLACE "(^| )[/-]D *NDEBUG($| )" "" ${_var} "${${_var}}")
  endforeach ()
endforeach ()

# C++ language standard
set(CMAKE_CXX_STANDARD 14)

# Toolchain
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")

# Java
find_package(Java COMPONENTS Runtime REQUIRED)

# ANTLR 4 Jar
if (NOT ANTLR_JAR_LOCATION)
  message(FATAL_ERROR "Missing antlr4.jar path. You can specify it's path using: -DANTLR_JAR_LOCATION=<path>")
endif ()
if (NOT IS_ABSOLUTE ${ANTLR_JAR_LOCATION})
  message(FATAL_ERROR "Please provide the full path to antlr4.jar")
endif ()
if (NOT EXISTS "${ANTLR_JAR_LOCATION}")
  message(FATAL_ERROR "Unable to find antlr4.jar in ${ANTLR_JAR_LOCATION}")
endif ()
get_filename_component(ANTLR_NAME ${ANTLR_JAR_LOCATION} NAME_WE)
message(STATUS "Found ${ANTLR_NAME}: ${ANTLR_JAR_LOCATION}")

# Python
find_program(PYTHON
  NAMES "python3"
  DOC "Python interpreter")
if (NOT PYTHON)
  message(FATAL_ERROR "Unable to find Python!")
else ()
  message(STATUS "Python: ${PYTHON}")
endif ()

# Generate "f1_c_fuzz.c" and "f1_c_fuzz.h"
# Check the input grammar file
if (NOT GRAMMAR_FILE)
  message(FATAL_ERROR "Missing the grammar file path. You can specify it's path using: -DGRAMMAR_FILE=<path>")
endif ()
if (NOT IS_ABSOLUTE ${GRAMMAR_FILE})
  message(FATAL_ERROR "Please provide the full path to the grammar file")
endif ()
if (NOT EXISTS ${GRAMMAR_FILE})
  message(FATAL_ERROR "The grammar file does not exist: ${GRAMMAR_FILE}")
endif ()
if (NOT GRAMMAR_FILENAME)
  get_filename_component(GRAMMAR_FILENAME ${GRAMMAR_FILE} NAME_WE)
  string(TOLOWER ${GRAMMAR_FILENAME} GRAMMAR_FILENAME)
  STRING(REGEX REPLACE "[_.-].*" "" GRAMMAR_FILENAME ${GRAMMAR_FILENAME})
  message(STATUS "Selected grammar name: ${GRAMMAR_FILENAME} (from ${GRAMMAR_FILE})")
else ()
  message(STATUS "Selected grammar name: ${GRAMMAR_FILENAME}")
endif ()


# Generate files at configure time
execute_process(
  COMMAND mkdir -p f1/src
  COMMAND mkdir -p f1/include
  COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/grammars/f1_c_gen.py ${GRAMMAR_FILE} ${CMAKE_BINARY_DIR}/f1
  COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/grammars/f1_g4_translate.py ${GRAMMAR_FILE} ${CMAKE_BINARY_DIR}/f1
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
if (result)
  message(FATAL_ERROR "CMake step for f1_c_fuzz and g4 translation failed: ${result}")
endif ()
set(GRAMMAR_G4_FILE "${CMAKE_BINARY_DIR}/f1/Grammar.g4")
if (EXISTS ${GRAMMAR_G4_FILE})
  message(STATUS "Grammar g4: ${GRAMMAR_G4_FILE}")
else ()
  message(FATAL_ERROR "The g4 grammar file does not exist: ${GRAMMAR_FILE}")
endif ()

add_subdirectory(src)
add_subdirectory(lib)
add_subdirectory(third_party)

if (ENABLE_TESTING)
  enable_testing()
  add_subdirectory(tests)

  # Valgrind
  add_custom_target(test_memcheck
    COMMAND ${CMAKE_CTEST_COMMAND} --force-new-ctest-process --test-action memcheck
    # Throw an error while finding any non-empty memory check reports
    # - `find` always returns zero, even if it does not find anything
    # - `grep .` will return non-zero, if no memory leaks are found
    COMMAND find "${CMAKE_BINARY_DIR}/Testing/Temporary" -name "MemoryChecker.*.log" -not -empty -exec cat {} + | grep . || exit 0 && exit 2)
endif ()
