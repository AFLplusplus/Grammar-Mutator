# Generate lexer and parser
execute_process(
  COMMAND ${Java_JAVA_EXECUTABLE} -jar ${ANTLR_JAR_LOCATION} -Dlanguage=Cpp -DcontextSuperClass=antlr4::RuleContextWithAltNum -o ${CMAKE_CURRENT_BINARY_DIR}/generated ${GRAMMAR_G4_FILE}
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
if (result)
  message(FATAL_ERROR "CMake step for ANTLR lexer and parser failed: ${result}")
endif ()

file(GLOB ANTLR4_GENERATED_SRC "${CMAKE_CURRENT_BINARY_DIR}/generated/*.cpp")
set(ANTLR4_RUNTIME_LIB_NAME libantlr4-runtime)
set(ANTLR4_RUNTIME_SRC_DIR ${CMAKE_BINARY_DIR}/third_party/antlr4-cpp-runtime/antlr4-cpp-runtime-src/runtime/src)
set(ANTLR4_RUNTIME_DIST_DIR ${CMAKE_BINARY_DIR}/third_party/antlr4-cpp-runtime/antlr4-cpp-runtime-src/dist)
#set(ANTLR4_SHARED_LIB ${ANTLR4_RUNTIME_DIST_DIR}/${ANTLR4_RUNTIME_LIB_NAME}.so)
set(ANTLR4_STATIC_LIB ${ANTLR4_RUNTIME_DIST_DIR}/${ANTLR4_RUNTIME_LIB_NAME}.a)

add_library(antlr4_shim STATIC
  antlr4_shim.cpp
  ${ANTLR4_GENERATED_SRC})
add_dependencies(antlr4_shim antlr4-cpp-runtime)
target_include_directories(antlr4_shim
  PRIVATE ${ANTLR4_RUNTIME_SRC_DIR}/
  PRIVATE ${ANTLR4_RUNTIME_SRC_DIR}/misc
  PRIVATE ${ANTLR4_RUNTIME_SRC_DIR}/atn
  PRIVATE ${ANTLR4_RUNTIME_SRC_DIR}/dfa
  PRIVATE ${ANTLR4_RUNTIME_SRC_DIR}/tree
  PRIVATE ${ANTLR4_RUNTIME_SRC_DIR}/support
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
  PRIVATE ${CMAKE_BINARY_DIR}/f1/include
  PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(antlr4_shim
  PRIVATE ${ANTLR4_STATIC_LIB})
set_target_properties(antlr4_shim PROPERTIES COMPILE_FLAGS -fPIC)
target_compile_options(antlr4_shim PRIVATE  -Wno-attributes)
