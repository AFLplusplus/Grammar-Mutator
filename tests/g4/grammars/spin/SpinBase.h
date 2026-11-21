/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The base class of the parser of the "spin" test grammar. Its predicate
   waits a fixed wall-clock time, so a parse takes a known minimum time on
   any machine.
 */

#ifndef __SPIN_BASE_H__
#define __SPIN_BASE_H__

#include "antlr4-runtime.h"

class SpinBase : public antlr4::Parser {

 public:
  // The time that one call of spin() waits, in microseconds
  static constexpr int SPIN_US = 100;

  explicit SpinBase(antlr4::TokenStream *input);

  /* Wait SPIN_US microseconds (a busy wait on a monotonic clock), and return
     true. */
  bool spin();

};

#endif
