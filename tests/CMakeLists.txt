# Download and unpack googletest at configure time
configure_file(
  googletest-download/CMakeLists.txt.in
  googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
if (result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif ()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
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
  COMMAND test_custom_mutator 100)

# Test suite 2:
# test the tree APIs
add_executable(test_tree test_tree.cpp)
target_link_libraries(test_tree
  PRIVATE gtest_main
  PRIVATE grammarmutator)
add_test(
  NAME test_tree
  COMMAND test_tree)

# Test suite 3:
# test the tree mutation APIs
add_executable(test_tree_mutation test_tree_mutation.cpp)
target_link_libraries(test_tree_mutation
  PRIVATE gtest_main
  PRIVATE grammarmutator)
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
# test the JSON grammar mutator with AFL++
if (${GRAMMAR_FILE} STREQUAL ${CMAKE_SOURCE_DIR}/grammars/json_grammar.json)
  find_program(AFL_FUZZ
    NAMES "afl-fuzz"
    DOC "american fuzzy lop")
  if (NOT AFL_FUZZ)
    message(STATUS "afl-fuzz not found!")
  else ()
    message(STATUS "afl-fuzz found: ${AFL_FUZZ}")
    set(AFL_FUZZ_OUT_DIR "/tmp/afl_fuzz_out")
    if (NOT EXISTS ${AFL_FUZZ_OUT_DIR})
      file(MAKE_DIRECTORY ${AFL_FUZZ_OUT_DIR})
      message(STATUS "Create afl-fuzz output directory: ${AFL_FUZZ_OUT_DIR}")
    else ()
      message(STATUS "afl-fuzz output directory: ${AFL_FUZZ_OUT_DIR}")
    endif ()

    # Download and unpack json-parser at configure time
    configure_file(
      json-parser-download/CMakeLists.txt.in
      json-parser-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/json-parser-download)
    if (result)
      message(FATAL_ERROR "CMake step for json-parser failed: ${result}")
    endif ()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/json-parser-download)
    if (result)
      message(FATAL_ERROR "Build step for json-parser failed: ${result}")
    endif ()
    set(FUZZING_TARGET ${CMAKE_CURRENT_BINARY_DIR}/json-parser-src/test_json)

    # Run afl-fuzz for 10 seconds, with fixed seed 0
    add_test(
      NAME fuzz_json_parser
      COMMAND ${AFL_FUZZ} -V 10 -s 0 -i ${CMAKE_SOURCE_DIR}/examples/JSON/in -o ${AFL_FUZZ_OUT_DIR} -- ${FUZZING_TARGET} @@)
    # Set environment variables
    set_property(
      TEST fuzz_json_parser
      PROPERTY ENVIRONMENT "AFL_SKIP_CPUFREQ=1")
    set_property(
      TEST fuzz_json_parser
      APPEND
      PROPERTY ENVIRONMENT "AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1")
    set_property(
      TEST fuzz_json_parser
      APPEND
      PROPERTY ENVIRONMENT "AFL_CUSTOM_MUTATOR_ONLY=1")
    set_property(
      TEST fuzz_json_parser
      APPEND
      PROPERTY ENVIRONMENT "AFL_CUSTOM_MUTATOR_LIBRARY=${CMAKE_BINARY_DIR}/src/libgrammarmutator.so")
  endif ()
endif ()

# Test suite 8:
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
