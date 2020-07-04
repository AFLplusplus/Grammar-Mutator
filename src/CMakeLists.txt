add_library(jsonmutator SHARED
  json_c_fuzz.c
  tree.c
  tree_mutation.c
  json_custom_mutator.cpp)
