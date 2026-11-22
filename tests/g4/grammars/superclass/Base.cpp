/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

#include <stdexcept>

#include "Base.h"

Base::Base(antlr4::TokenStream *input) : antlr4::Parser(input) {

}

bool Base::check() {

  if (_input->LT(1)->getText().rfind('t', 0) == 0) {

    throw std::runtime_error("the input starts with t");

  }

  return true;

}
