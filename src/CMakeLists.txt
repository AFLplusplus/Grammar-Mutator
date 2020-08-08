add_library(grammarmutator SHARED
  chunk_store.cpp
  list.c
  tree.c
  tree_mutation.c
  tree_trimming.c
  ${CMAKE_BINARY_DIR}/f1/src/f1_c_fuzz.c
  grammar_mutator.c)
