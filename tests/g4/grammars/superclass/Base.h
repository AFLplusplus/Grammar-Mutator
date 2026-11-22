/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The base class of the parser of the "superclass" test grammar. Its
   predicate throws a C++ exception that is not an ANTLR exception.
 */

#ifndef __BASE_H__
#define __BASE_H__

#include "antlr4-runtime.h"

class Base : public antlr4::Parser {

 public:
  explicit Base(antlr4::TokenStream *input);

  /* Throw std::runtime_error if the next token starts with "t". Otherwise,
     return true. */
  bool check();

};

#endif
