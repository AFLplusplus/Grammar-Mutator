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
   rule entry or an LA() call. In the full-context prediction of the ANTLR
   4.13.2 runtime, only the closure step reads the clock (see TimedSimulator).
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

  // While this is above 0, check() does not throw, because a throw at that
  // point leaks memory in the runtime (see TimedSimulator).
  mutable int quiet = 0;

  // Return true after the deadline. Return false while the stack unwinds for
  // a different exception.
  bool due() const {

    return deadline && std::uncaught_exceptions() == 0 &&
           parse_budget_now_ns() > deadline;

  }

  // Throw StopParse after the deadline, unless the clock is quiet
  void check() const {

    if (quiet == 0 && due()) throw StopParse();

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
// closure step.
//
// execATNWithFullContext of the ANTLR 4.13.2 runtime keeps the configuration
// set of the previous step in a raw pointer, and does not free it if an
// exception passes.
// So in that function, the clock checks of LA() and of rule entries are quiet.
// The closure step still throws, and computeReachSet then frees that set
// before the exception (or any other exception) goes on.
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
  size_t execATNWithFullContext(antlr4::dfa::DFA            &dfa,
                                antlr4::dfa::DFAState       *D,
                                antlr4::atn::ATNConfigSet   *s0,
                                antlr4::TokenStream         *input,
                                size_t                       startIndex,
                                antlr4::ParserRuleContext   *outerContext)
      override {

    // Keep the start set of an outer call, and restore it on every exit
    struct Scope {

      TimedSimulator            *sim;
      antlr4::atn::ATNConfigSet *saved_s0;
      ~Scope() {

        sim->full_ctx_s0_ = saved_s0;
        --sim->clock_.quiet;

      }

    } scope{this, full_ctx_s0_};

    full_ctx_s0_ = s0;
    ++clock_.quiet;
    return antlr4::atn::ParserATNSimulator::execATNWithFullContext(
        dfa, D, s0, input, startIndex, outerContext);

  }

  std::unique_ptr<antlr4::atn::ATNConfigSet> computeReachSet(
      antlr4::atn::ATNConfigSet *closure, size_t t, bool fullCtx) override {

    try {

      return antlr4::atn::ParserATNSimulator::computeReachSet(closure, t,
                                                              fullCtx);

    } catch (...) {

      // In execATNWithFullContext, "closure" is the start set (the caller owns
      // it) or the set of the previous step (nobody frees it after a throw)
      if (fullCtx && full_ctx_s0_ && closure != full_ctx_s0_) delete closure;
      throw;

    }

  }

  void closure_(Ref<antlr4::atn::ATNConfig> const &config,
                antlr4::atn::ATNConfigSet        *configs,
                antlr4::atn::ATNConfig::Set &closureBusy, bool collectPredicates,
                bool fullCtx, int depth, bool treatEofAsEpsilon) override {

    if ((++calls_ & 255) == 0 && clock_.due()) throw StopParse();
    antlr4::atn::ParserATNSimulator::closure_(config, configs, closureBusy,
                                              collectPredicates, fullCtx, depth,
                                              treatEofAsEpsilon);

  }

 private:
  const Clock               &clock_;
  size_t                     calls_ = 0;
  antlr4::atn::ATNConfigSet *full_ctx_s0_ = nullptr;

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
