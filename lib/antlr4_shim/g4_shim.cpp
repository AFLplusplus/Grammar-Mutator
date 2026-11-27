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

#include <antlr4-runtime.h>

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

bool tree_parser_cache_states(size_t *parser_states, size_t *lexer_states) {

  if (!parser_states || !lexer_states) return false;

  try {

    // A lexer and a parser on an empty input. Their simulators refer to the
    // shared static caches. This changes no cache.
    antlr4::ANTLRInputStream input("");
    auto lexer = g4build::grammar.make_lexer(&input);
    if (!lexer) return false;
    antlr4::CommonTokenStream tokens(lexer.get());
    auto parser = g4build::grammar.make_parser(&tokens);
    if (!parser) return false;

    size_t count = 0;
    for (auto &dfa :
         parser->getInterpreter<antlr4::atn::ParserATNSimulator>()
             ->decisionToDFA)
      count += dfa.states.size();
    *parser_states = count;

    count = 0;
    for (auto &dfa :
         lexer->getInterpreter<antlr4::atn::LexerATNSimulator>()
             ->_decisionToDFA)
      count += dfa.states.size();
    *lexer_states = count;

  } catch (std::exception &) {

    return false;

  }

  return true;

}
