add_library(grammarmutator SHARED
  chunk_store.cpp
  list.c
  tree.c
  tree_mutation.c
  tree_trimming.c
  f1_c_fuzz.c
  grammar_mutator.cpp)
