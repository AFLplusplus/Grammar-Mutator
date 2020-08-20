add_subdirectory(rxi_map)
add_subdirectory(barrust_set)

# Download and unpack antlr4-cpp-runtime at configure time
configure_file(
  antlr4-cpp-runtime/CMakeLists.txt.in
  antlr4-cpp-runtime/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/antlr4-cpp-runtime)
if (result)
  message(FATAL_ERROR "CMake step for antlr4-cpp-runtime failed: ${result}")
endif ()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/antlr4-cpp-runtime)
if (result)
  message(FATAL_ERROR "Build step for antlr4-cpp-runtime failed: ${result}")
endif ()

set(ANTLR4_RUNTIME_LIB_NAME libantlr4-runtime)
set(ANTLR4_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/antlr4-cpp-runtime-src/dist)
#set(ANTLR4_SHARED_LIB ${ANTLR4_OUTPUT_DIR}/${ANTLR4_RUNTIME_LIB_NAME}.so)
set(ANTLR4_STATIC_LIB ${ANTLR4_OUTPUT_DIR}/${ANTLR4_RUNTIME_LIB_NAME}.a)

if (NOT EXISTS "${ANTLR4_STATIC_LIB}")
  message(FATAL_ERROR "Unable to find ${ANTLR4_RUNTIME_LIB_NAME}.a")
else ()
  message(STATUS "Found ${ANTLR4_RUNTIME_LIB_NAME}.a: ${ANTLR4_STATIC_LIB}")
endif ()
