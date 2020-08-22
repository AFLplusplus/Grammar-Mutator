add_library(grammarmutator SHARED
  chunk_store.c
  list.c
  tree.c
  tree_mutation.c
  tree_trimming.c
  ${CMAKE_BINARY_DIR}/f1/src/f1_c_fuzz.c
  grammar_mutator.c)

target_link_libraries(grammarmutator
  PRIVATE rxi_map
  PRIVATE barrust_set
  PUBLIC antlr4_shim)
target_include_directories(grammarmutator
  PUBLIC ${CMAKE_SOURCE_DIR}/include
  PUBLIC ${CMAKE_BINARY_DIR}/f1/include  # Generated headers
  PRIVATE ${CMAKE_SOURCE_DIR}/third_party/rxi_map
  PRIVATE ${CMAKE_SOURCE_DIR}/third_party/barrust_set)

add_executable(grammar_generator
  grammar_generator.c)
target_link_libraries(grammar_generator
  PRIVATE grammarmutator)
target_include_directories(grammarmutator
  PUBLIC ${CMAKE_SOURCE_DIR}/include
  PUBLIC ${CMAKE_BINARY_DIR}/f1/include)
