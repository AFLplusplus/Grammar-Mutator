/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

#include "TokenBase.h"

TokenBase::TokenBase(antlr4::CharStream *input) : antlr4::Lexer(input) {

}

std::unique_ptr<antlr4::Token> TokenBase::nextToken() {

  if (!made_) {

    made_ = true;
    // ID is the first token type of the lexer grammar, so its type is 1.
    return std::make_unique<antlr4::CommonToken>(1, "x");

  }

  return antlr4::Lexer::nextToken();

}
