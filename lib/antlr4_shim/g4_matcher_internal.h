/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The interface between the two parts of the matcher. g4_matcher_antlr.cpp
   runs the ANTLR lexer and parser. g4_matcher.cpp changes the parse tree into
   a tree. Only these two files use this header.
 */

#ifndef __G4_MATCHER_INTERNAL_H__
#define __G4_MATCHER_INTERNAL_H__

#include <cstddef>
#include <cstdint>
#include <vector>

#include "g4_matcher.h"

// The ANTLR classes that this header uses. The users include the runtime.
namespace antlr4 {

class Parser;
class ParserRuleContext;

}  // namespace antlr4

namespace g4 {

namespace detail {

/* Change the parse tree root_ctx of buf (len bytes) into a tree of grammar.
   parser is the parser that made root_ctx. offsets holds the byte offset of
   each code point of buf, and len at the end. token_count is the number of
   tokens in the token stream. Return the tree, or nullptr if the parse tree
   does not match the tables. Throw std::bad_alloc if an allocation fails. */
tree_t *match_tree(const Grammar &grammar, const antlr4::Parser &parser,
                   antlr4::ParserRuleContext *root_ctx, const uint8_t *buf,
                   size_t len, const std::vector<size_t> &offsets,
                   size_t token_count);

}  // namespace detail

}  // namespace g4

#endif
