# A program to benchmark the grammar mutator
add_executable(benchmark
  benchmark.c)
target_link_libraries(benchmark
  PRIVATE grammarmutator)
target_include_directories(benchmark
  PUBLIC ${CMAKE_SOURCE_DIR}/include
  PUBLIC ${CMAKE_BINARY_DIR}/f1/include)
