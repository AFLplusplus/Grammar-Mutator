/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

#include <chrono>

#include "SpinBase.h"

SpinBase::SpinBase(antlr4::TokenStream *input) : antlr4::Parser(input) {

}

bool SpinBase::spin() {

  auto end =
      std::chrono::steady_clock::now() + std::chrono::microseconds(SPIN_US);
  while (std::chrono::steady_clock::now() < end) {}

  return true;

}
