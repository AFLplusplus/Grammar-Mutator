/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The parse clock: the checks that stop an ANTLR parse after a deadline. The
   JSON shim (antlr4_shim.cpp) and the .g4 matcher (g4_matcher_antlr.cpp) use
   them.

   The parser reads the clock at each rule entry, in every 1,024th LA() call,
   and in every 256th call of the closure step of the prediction. With an
   ambiguous grammar, the closure step can do a large amount of work without a
   rule entry or an LA() call.
 */

#ifndef __PARSE_CLOCK_H__
#define __PARSE_CLOCK_H__

#include <cstdint>
#include <exception>
#include <memory>

#include "antlr4-runtime.h"

#include "parse_budget.h"

namespace parse_clock {

// The exception that stops a parse at the deadline. It does not derive from
// antlr4::RecognitionException, so the generated rule code does not catch it.
struct StopParse {};

// A deadline on the clock of parse_budget_now_ns()
struct Clock {

  // The deadline, or 0 for no deadline
  uint64_t deadline = 0;

  // Throw StopParse after the deadline. Do not throw while the stack unwinds
  // for a different exception.
  void check() const {

    if (deadline && std::uncaught_exceptions() == 0 &&
        parse_budget_now_ns() > deadline) {

      throw StopParse();

    }

  }

};

// Reads the clock at each rule entry
class ClockListener : public antlr4::tree::ParseTreeListener {

 public:
  explicit ClockListener(const Clock &clock) : clock_(clock) {

  }

  void enterEveryRule(antlr4::ParserRuleContext *) override {

    clock_.check();

  }

  // The runtime calls this while the stack unwinds. It must not throw.
  void exitEveryRule(antlr4::ParserRuleContext *) override {

  }

  void visitTerminal(antlr4::tree::TerminalNode *) override {

  }

  void visitErrorNode(antlr4::tree::ErrorNode *) override {

  }

 private:
  const Clock &clock_;

};

// A token stream that reads the clock once every 1,024 LA() calls. The
// prediction code can run a long time inside one rule.
class TimedTokenStream : public antlr4::CommonTokenStream {

 public:
  using antlr4::CommonTokenStream::CommonTokenStream;

  const Clock *clock = nullptr;

  size_t LA(ssize_t i) override {

    if (clock && (++calls_ & 1023) == 0) clock->check();
    return antlr4::CommonTokenStream::LA(i);

  }

 private:
  size_t calls_ = 0;

};

// A prediction simulator that reads the clock once every 256 calls of the
// closure step
class TimedSimulator : public antlr4::atn::ParserATNSimulator {

 public:
  // Use the ATN, the DFA list, and the context cache of the current simulator
  // of "parser", as Parser::setProfile does
  TimedSimulator(antlr4::Parser *parser, const Clock &clock)
      : antlr4::atn::ParserATNSimulator(
            parser, parser->getATN(),
            parser->getInterpreter<antlr4::atn::ParserATNSimulator>()
                ->decisionToDFA,
            parser->getInterpreter<antlr4::atn::ParserATNSimulator>()
                ->getSharedContextCache()),
        clock_(clock) {

  }

 protected:
  void closure_(Ref<antlr4::atn::ATNConfig> const &config,
                antlr4::atn::ATNConfigSet        *configs,
                antlr4::atn::ATNConfig::Set &closureBusy, bool collectPredicates,
                bool fullCtx, int depth, bool treatEofAsEpsilon) override {

    if ((++calls_ & 255) == 0) clock_.check();
    antlr4::atn::ParserATNSimulator::closure_(config, configs, closureBusy,
                                              collectPredicates, fullCtx, depth,
                                              treatEofAsEpsilon);

  }

 private:
  const Clock &clock_;
  size_t       calls_ = 0;

};

// Give "parser" a TimedSimulator with the same prediction mode. The parser
// deletes its old simulator.
inline void install_timed_simulator(antlr4::Parser &parser,
                                    const Clock    &clock) {

  antlr4::atn::PredictionMode mode =
      parser.getInterpreter<antlr4::atn::ParserATNSimulator>()
          ->getPredictionMode();
  parser.setInterpreter(new TimedSimulator(&parser, clock));
  parser.getInterpreter<antlr4::atn::ParserATNSimulator>()->setPredictionMode(
      mode);

}

}  // namespace parse_clock

#endif
