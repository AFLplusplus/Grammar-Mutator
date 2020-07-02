add_library(jsonmutator SHARED
  json_c_fuzz.c
  parsing_tree.c
  json_custom_mutator.cpp
)
