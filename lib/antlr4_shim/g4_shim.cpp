/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The parser of a .g4 build. The build writes the tables of the grammar
   (g4_grammar.cpp) in the namespace g4build. The matcher (g4_matcher.cpp
   and g4_matcher_antlr.cpp) parses with them.

   The ANTLR lexer and parser stay in the global namespace (the build does
   not use the jar option -package). The C++ base classes of grammars-v4 use
   the names of the generated classes without a namespace.
 */

#include "g4_matcher.h"

namespace g4build {

extern const g4::Grammar grammar;

}  // namespace g4build

tree_t *tree_parse(const uint8_t *buf, size_t len, parse_budget_t *budget,
                   parse_status_t *status) {

  g4::Limits limits;
  limits.budget = budget;
  return g4::parse(g4build::grammar, buf, len, limits, status);

}
