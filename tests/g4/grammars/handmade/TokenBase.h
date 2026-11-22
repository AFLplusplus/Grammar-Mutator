/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The base class of the lexer of the "handmade" test grammar. Before the
   first real token, it gives one token that it makes itself, as a lexer base
   class can do. That token has the type of ID, but no position in the input:
   its start and stop indexes are 0.
 */

#ifndef __TOKEN_BASE_H__
#define __TOKEN_BASE_H__

#include <memory>

#include "antlr4-runtime.h"

class TokenBase : public antlr4::Lexer {

 public:
  explicit TokenBase(antlr4::CharStream *input);

  std::unique_ptr<antlr4::Token> nextToken() override;

 private:
  bool made_ = false;

};

#endif
