include(ExternalProject)
ExternalProject_Add(antlr4-cpp-runtime
  URL https://www.antlr.org/download/antlr4-cpp-runtime-4.8-source.zip
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/antlr4-cpp-runtime-src"
  BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/antlr4-cpp-runtime-build"
  CONFIGURE_COMMAND ${CMAKE_COMMAND} ${CMAKE_CURRENT_BINARY_DIR}/antlr4-cpp-runtime-src -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_CXX_FLAGS=-fPIC
  BUILD_COMMAND ${CMAKE_COMMAND} --build .
  INSTALL_COMMAND ""
  TEST_COMMAND ""
  )
