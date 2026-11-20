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

# Do not disable assertions based on CMAKE_BUILD_TYPE.
foreach (_build_type "Release" "MinSizeRel" "RelWithDebInfo")
  foreach (_lang C CXX)
    string(TOUPPER "CMAKE_${_lang}_FLAGS_${_build_type}" _var)
    string(REGEX REPLACE "(^| )[/-]D *NDEBUG($| )" "" ${_var} "${${_var}}")
  endforeach ()
endforeach ()

# C++ language standard (the ANTLR 4.13 C++ runtime needs C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ANTLR version. The ANTLR jar must have the same version as the C++ runtime.
set(ANTLR_VERSION 4.13.2)
set(ANTLR_RUNTIME_SHA256 0ed13668906e86dbc0dcddf30fdee68c10203dea4e83852b4edb810821bee3c4)

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
execute_process(
  COMMAND ${Java_JAVA_EXECUTABLE} -jar ${ANTLR_JAR_LOCATION}
  OUTPUT_VARIABLE ANTLR_JAR_OUTPUT
  ERROR_QUIET)
string(REGEX MATCH "Version ([0-9.]+)" _ "${ANTLR_JAR_OUTPUT}")
if (NOT "${CMAKE_MATCH_1}" STREQUAL "${ANTLR_VERSION}")
  message(FATAL_ERROR "The ANTLR jar has version '${CMAKE_MATCH_1}', but the build needs ${ANTLR_VERSION}: ${ANTLR_JAR_LOCATION}")
endif ()

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
# Check the input grammar files. GRAMMAR_FILE is one .json file, or a list of
# ANTLR 4 .g4 files (for example "/path/Lexer.g4;/path/Parser.g4").
if (NOT GRAMMAR_FILE)
  message(FATAL_ERROR "Missing the grammar file path. You can specify it's path using: -DGRAMMAR_FILE=<path>")
endif ()
set(G4_FILES "")
foreach (_grammar_file IN LISTS GRAMMAR_FILE)
  if (NOT IS_ABSOLUTE ${_grammar_file})
    message(FATAL_ERROR "Please provide the full path to the grammar file: ${_grammar_file}")
  endif ()
  if (NOT EXISTS ${_grammar_file})
    message(FATAL_ERROR "The grammar file does not exist: ${_grammar_file}")
  endif ()
  if (_grammar_file MATCHES "\\.g4$")
    list(APPEND G4_FILES ${_grammar_file})
  endif ()
endforeach ()
list(LENGTH GRAMMAR_FILE _grammar_file_count)
list(LENGTH G4_FILES _g4_file_count)
if (_g4_file_count GREATER 0 AND NOT _g4_file_count EQUAL _grammar_file_count)
  message(FATAL_ERROR "Do not mix .g4 files with other grammar files: ${GRAMMAR_FILE}")
endif ()
if (_g4_file_count EQUAL 0 AND _grammar_file_count GREATER 1)
  message(FATAL_ERROR "Give one JSON grammar file: ${GRAMMAR_FILE}")
endif ()
if (_g4_file_count EQUAL 0 AND GRAMMAR_START)
  message(FATAL_ERROR "GRAMMAR_START works only with .g4 grammar files")
endif ()
# Configure again when a grammar file changes
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GRAMMAR_FILE})

if (NOT GRAMMAR_FILENAME)
  if (_g4_file_count GREATER 0)
    # For example, JavaScriptLexer.g4 and JavaScriptParser.g4 give "javascript"
    list(GET G4_FILES -1 _last_g4_file)
    get_filename_component(GRAMMAR_FILENAME ${_last_g4_file} NAME_WE)
    string(REGEX REPLACE "(Parser|Lexer)$" "" GRAMMAR_FILENAME ${GRAMMAR_FILENAME})
    string(TOLOWER ${GRAMMAR_FILENAME} GRAMMAR_FILENAME)
  else ()
    get_filename_component(GRAMMAR_FILENAME ${GRAMMAR_FILE} NAME_WE)
    string(TOLOWER ${GRAMMAR_FILENAME} GRAMMAR_FILENAME)
    STRING(REGEX REPLACE "[_.-].*" "" GRAMMAR_FILENAME ${GRAMMAR_FILENAME})
  endif ()
  message(STATUS "Selected grammar name: ${GRAMMAR_FILENAME} (from ${GRAMMAR_FILE})")
else ()
  message(STATUS "Selected grammar name: ${GRAMMAR_FILENAME}")
endif ()

# Generate files at configure time
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/f1/src ${CMAKE_BINARY_DIR}/f1/include)
if (_g4_file_count GREATER 0)
  # The Python tools (requirements.txt) in a virtual environment in the build
  # folder
  set(GRAMMAR_VENV ${CMAKE_BINARY_DIR}/venv)
  if (NOT EXISTS ${GRAMMAR_VENV}/bin/python)
    execute_process(
      COMMAND ${PYTHON} -m venv ${GRAMMAR_VENV}
      RESULT_VARIABLE result)
    if (result)
      message(FATAL_ERROR "Unable to create the Python virtual environment: ${result}")
    endif ()
  endif ()
  execute_process(
    COMMAND ${GRAMMAR_VENV}/bin/pip install --quiet -r ${CMAKE_SOURCE_DIR}/requirements.txt
    RESULT_VARIABLE result)
  if (result)
    message(FATAL_ERROR "Unable to install the Python tools (requirements.txt): ${result}")
  endif ()
  # The Python parser for .g4 files, from the ANTLR 4 meta-grammar
  execute_process(
    COMMAND ${Java_JAVA_EXECUTABLE} -jar ${ANTLR_JAR_LOCATION} -Dlanguage=Python3
            -Xexact-output-dir -o ${CMAKE_BINARY_DIR}/antlr4_meta
            ${CMAKE_SOURCE_DIR}/grammars/antlr4/ANTLRv4Lexer.g4
            ${CMAKE_SOURCE_DIR}/grammars/antlr4/ANTLRv4Parser.g4
    RESULT_VARIABLE result)
  if (result)
    message(FATAL_ERROR "Unable to generate the parser for .g4 files: ${result}")
  endif ()
  set(_start_option "")
  if (GRAMMAR_START)
    set(_start_option --start ${GRAMMAR_START})
  endif ()
  set(GRAMMAR_JSON ${CMAKE_BINARY_DIR}/f1/grammar.json)
  set(PARSE_MAP ${CMAKE_BINARY_DIR}/f1/parse_map.json)
  execute_process(
    COMMAND ${GRAMMAR_VENV}/bin/python ${CMAKE_SOURCE_DIR}/grammars/g4_to_json.py
            ${_start_option} --meta-dir ${CMAKE_BINARY_DIR}/antlr4_meta
            --parse-map ${PARSE_MAP} -o ${GRAMMAR_JSON} ${G4_FILES}
    RESULT_VARIABLE result)
  if (result)
    message(FATAL_ERROR "Unable to convert the .g4 grammar: ${result}")
  endif ()

  # The Python parser for test cases. G4_SUPPORT_DIR is the folder with the
  # base classes and the rewrite script of a grammar with action code, for
  # example grammars-v4 javascript/javascript/Python3.
  set(PARSER_DIR ${CMAKE_BINARY_DIR}/g4_python)
  file(REMOVE_RECURSE ${PARSER_DIR})
  file(MAKE_DIRECTORY ${PARSER_DIR})
  file(COPY ${G4_FILES} DESTINATION ${PARSER_DIR})
  if (G4_SUPPORT_DIR)
    if (NOT IS_ABSOLUTE "${G4_SUPPORT_DIR}")
      message(FATAL_ERROR "Please provide the full path to G4_SUPPORT_DIR: ${G4_SUPPORT_DIR}")
    endif ()
    if (NOT IS_DIRECTORY "${G4_SUPPORT_DIR}")
      message(FATAL_ERROR "G4_SUPPORT_DIR is not a folder: ${G4_SUPPORT_DIR}")
    endif ()
    file(GLOB _support_files "${G4_SUPPORT_DIR}/*")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_support_files})
    if (EXISTS "${G4_SUPPORT_DIR}/transformGrammar.py")
      execute_process(
        COMMAND ${GRAMMAR_VENV}/bin/python ${G4_SUPPORT_DIR}/transformGrammar.py
        WORKING_DIRECTORY ${PARSER_DIR}
        RESULT_VARIABLE result)
      if (result)
        message(FATAL_ERROR "Unable to run transformGrammar.py: ${result}")
      endif ()
    endif ()
    file(GLOB _support_py "${G4_SUPPORT_DIR}/*.py")
    list(REMOVE_ITEM _support_py "${G4_SUPPORT_DIR}/transformGrammar.py")
    if (_support_py)
      file(COPY ${_support_py} DESTINATION ${PARSER_DIR})
    endif ()
  endif ()
  file(GLOB _work_g4_files "${PARSER_DIR}/*.g4")
  execute_process(
    COMMAND ${Java_JAVA_EXECUTABLE} -jar ${ANTLR_JAR_LOCATION} -Dlanguage=Python3
            -Xexact-output-dir -o ${PARSER_DIR} ${_work_g4_files}
    RESULT_VARIABLE result)
  if (result)
    message(FATAL_ERROR "Unable to generate the Python parser: ${result}")
  endif ()
  # The launcher of the parser. It runs grammars/g4_parse.py.
  set(VENV_PYTHON ${GRAMMAR_VENV}/bin/python)
  set(G4_PARSE ${CMAKE_SOURCE_DIR}/grammars/g4_parse.py)
  configure_file(${CMAKE_SOURCE_DIR}/grammars/grammar_parser.in
                 ${CMAKE_BINARY_DIR}/src/grammar_parser-${GRAMMAR_FILENAME}
                 @ONLY
                 FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                                  GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
               ${CMAKE_SOURCE_DIR}/grammars/grammar_parser.in)

  # The shim gets a parser that rejects all input. grammar_parser-<name>
  # parses the test cases.
  set(GRAMMAR_G4_FILE "${CMAKE_SOURCE_DIR}/grammars/g4_placeholder/Grammar.g4")
else ()
  if (G4_SUPPORT_DIR)
    message(FATAL_ERROR "G4_SUPPORT_DIR works only with .g4 grammar files")
  endif ()
  set(GRAMMAR_JSON ${GRAMMAR_FILE})
  execute_process(
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/grammars/f1_g4_translate.py ${GRAMMAR_JSON} ${CMAKE_BINARY_DIR}/f1
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  if (result)
    message(FATAL_ERROR "CMake step for g4 translation failed: ${result}")
  endif ()
  set(GRAMMAR_G4_FILE "${CMAKE_BINARY_DIR}/f1/Grammar.g4")
endif ()
execute_process(
  COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/grammars/f1_c_gen.py ${GRAMMAR_JSON} ${CMAKE_BINARY_DIR}/f1
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
if (result)
  message(FATAL_ERROR "CMake step for f1_c_fuzz failed: ${result}")
endif ()
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
