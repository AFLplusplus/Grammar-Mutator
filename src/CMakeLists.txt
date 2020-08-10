add_library(grammarmutator SHARED
  chunk_store.cpp
  list.c
  tree.c
  tree_mutation.c
  tree_trimming.c
  ${CMAKE_BINARY_DIR}/f1/src/f1_c_fuzz.c
  grammar_mutator.c)

target_link_libraries(grammarmutator
  PRIVATE rxi_map)
target_include_directories(grammarmutator
  PUBLIC ${CMAKE_SOURCE_DIR}/include
  PUBLIC ${CMAKE_BINARY_DIR}/f1/include)  # Generated headers
