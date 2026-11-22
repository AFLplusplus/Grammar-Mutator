set(ANTLR4_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(ANTLR4_RUNTIME_SRC_DIR ${CMAKE_BINARY_DIR}/third_party/antlr4-cpp-runtime/antlr4-cpp-runtime-src/runtime/src)
set(ANTLR4_STATIC_LIB ${CMAKE_BINARY_DIR}/third_party/antlr4-cpp-runtime/antlr4-cpp-runtime-build/runtime/libantlr4-runtime.a)

# The generated files of an earlier configure step can be from another grammar
file(REMOVE_RECURSE ${ANTLR4_GENERATED_DIR})

if (G4_BUILD)
  # A .g4 build: the C++ base classes of G4_SUPPORT_DIR, the lexer and parser
  # from the .g4 files in the work folder, and the parse tables
  # (g4_grammar.cpp). Without G4_SUPPORT_DIR, a grammar with action code fails
  # at the C++ compile step with a hint. The lexer and parser stay in the
  # global namespace (no -package), because the base classes use their names
  # without a namespace.
  file(MAKE_DIRECTORY ${ANTLR4_GENERATED_DIR})
  if (G4_SUPPORT_DIR)
    file(GLOB _support_sources "${G4_SUPPORT_DIR}/*.cpp" "${G4_SUPPORT_DIR}/*.h")
    if (_support_sources)
      file(COPY ${_support_sources} DESTINATION ${ANTLR4_GENERATED_DIR})
    endif ()
  endif ()
  execute_process(
    COMMAND ${Java_JAVA_EXECUTABLE} -jar ${ANTLR_JAR_LOCATION} -Dlanguage=Cpp -Xexact-output-dir
            -o ${ANTLR4_GENERATED_DIR} ${G4_WORK_FILES}
    RESULT_VARIABLE result)
  if (result)
    message(FATAL_ERROR "CMake step for ANTLR lexer and parser failed: ${result}")
  endif ()
  execute_process(
    COMMAND ${GRAMMAR_VENV}/bin/python ${CMAKE_SOURCE_DIR}/grammars/g4_c_gen.py
            --grammar ${GRAMMAR_JSON} --parse-map ${PARSE_MAP} --namespace g4build
            -o ${ANTLR4_GENERATED_DIR}
    RESULT_VARIABLE result)
  if (result)
    message(FATAL_ERROR "Unable to write the parse tables: ${result}")
  endif ()
  file(GLOB ANTLR4_GENERATED_SRC "${ANTLR4_GENERATED_DIR}/*.cpp")

  # g4_compile.sh prints a hint about G4_SUPPORT_DIR if a compile fails. It
  # prints the hint once and makes the folder G4_HINT_MARK. Each build removes
  # that folder first.
  set(G4_HINT_MARK ${ANTLR4_GENERATED_DIR}/.g4_compile_hint)
  add_custom_target(g4_hint_reset COMMAND rm -rf ${G4_HINT_MARK})
  add_library(antlr4_shim_generated OBJECT ${ANTLR4_GENERATED_SRC})
  add_dependencies(antlr4_shim_generated antlr4-cpp-runtime g4_hint_reset)
  target_include_directories(antlr4_shim_generated
    PRIVATE ${ANTLR4_RUNTIME_SRC_DIR}
    PRIVATE ${ANTLR4_GENERATED_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/include)
  set_target_properties(antlr4_shim_generated PROPERTIES
    COMPILE_FLAGS -fPIC
    CXX_COMPILER_LAUNCHER "sh;${CMAKE_CURRENT_SOURCE_DIR}/g4_compile.sh;${G4_HINT_MARK}")
  target_compile_options(antlr4_shim_generated PRIVATE -Wno-attributes)

  add_library(antlr4_shim STATIC
    g4_shim.cpp
    g4_matcher.cpp
    g4_matcher_antlr.cpp
    $<TARGET_OBJECTS:antlr4_shim_generated>)
  target_link_libraries(antlr4_shim
    PRIVATE ${ANTLR4_STATIC_LIB}
    PUBLIC Threads::Threads)
else ()
  # Generate lexer and parser
  execute_process(
    COMMAND ${Java_JAVA_EXECUTABLE} -jar ${ANTLR_JAR_LOCATION} -Dlanguage=Cpp -DcontextSuperClass=antlr4::RuleContextWithAltNum -o ${ANTLR4_GENERATED_DIR} ${GRAMMAR_G4_FILE}
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  if (result)
    message(FATAL_ERROR "CMake step for ANTLR lexer and parser failed: ${result}")
  endif ()
  file(GLOB ANTLR4_GENERATED_SRC "${ANTLR4_GENERATED_DIR}/*.cpp")

  add_library(antlr4_shim STATIC
    antlr4_shim.cpp
    ${ANTLR4_GENERATED_SRC})
  target_link_libraries(antlr4_shim
    PRIVATE ${ANTLR4_STATIC_LIB})
endif ()

add_dependencies(antlr4_shim antlr4-cpp-runtime)
target_include_directories(antlr4_shim
  PRIVATE ${ANTLR4_RUNTIME_SRC_DIR}
  PRIVATE ${ANTLR4_GENERATED_DIR}
  PRIVATE ${CMAKE_BINARY_DIR}/f1/include
  PUBLIC ${CMAKE_SOURCE_DIR}/include)
set_target_properties(antlr4_shim PROPERTIES COMPILE_FLAGS -fPIC)
target_compile_options(antlr4_shim PRIVATE  -Wno-attributes)
