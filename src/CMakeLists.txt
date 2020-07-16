add_library(jsonmutator SHARED
  chunk_store.cpp
  list.c
  tree.c
  tree_mutation.c
  tree_trimming.c
  json_c_fuzz.c
  json_custom_mutator.cpp)
