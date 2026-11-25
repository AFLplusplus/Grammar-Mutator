/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The ANTLR glue of the matcher: parse one input with the ANTLR C++ lexer and
   parser of a .g4 grammar, and give the parse tree to the matcher
   (g4_matcher.cpp).

   The ANTLR runtime parses with recursion, so the parse runs in a thread with
   a large stack, and a rule-depth limit stops a parse that is too deep. A
   time budget stops a parse that is too slow.
 */

#include <pthread.h>

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "antlr4-runtime.h"
#include "support/Utf8.h"

#include "g4_matcher.h"
#include "g4_matcher_internal.h"
#include "parse_clock.h"

namespace g4 {

namespace {

// The exception that stops a parse at the depth limit or at the deadline
using parse_clock::StopParse;

// -- ANTLR glue --------------------------------------------------------------

// An input stream with the exact code points. ANTLRInputStream::load removes
// a UTF-8 BOM, but the tree must keep it.
class CodePointStream : public antlr4::ANTLRInputStream {

 public:
  explicit CodePointStream(std::u32string code_points) {

    _data = std::move(code_points);

  }

};

// Counts the errors of the lexer or the parser, and prints nothing
class ErrorCounter : public antlr4::BaseErrorListener {

 public:
  size_t errors = 0;

  void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t, size_t,
                   const std::string &, std::exception_ptr) override {

    ++errors;

  }

};

// Counts the rule depth and reads the clock. Throws StopParse above the
// depth limit or after the deadline.
class Guard : public antlr4::tree::ParseTreeListener {

 public:
  explicit Guard(size_t max_depth) : max_depth_(max_depth) {

  }

  // The deadline of the parse
  parse_clock::Clock clock;

  void enterEveryRule(antlr4::ParserRuleContext *) override {

    if (++depth_ > max_depth_ && std::uncaught_exceptions() == 0) {

      throw StopParse();

    }

    clock.check();

  }

  // The runtime calls this while the stack unwinds. It must not throw.
  void exitEveryRule(antlr4::ParserRuleContext *) override {

    if (depth_) --depth_;

  }

  void visitTerminal(antlr4::tree::TerminalNode *) override {

  }

  void visitErrorNode(antlr4::tree::ErrorNode *) override {

  }

 private:
  size_t max_depth_;
  size_t depth_ = 0;

};

// -- one parse ---------------------------------------------------------------

// Return true if visible tokens follow the end of the start rule. ANTLR stops
// after the start rule without an error, also if visible tokens follow it (a
// start rule without EOF).
bool has_trailing_tokens(antlr4::CommonTokenStream &tokens,
                         antlr4::ParserRuleContext *root_ctx) {

  size_t first = root_ctx->stop ? root_ctx->stop->getTokenIndex() + 1 : 0;
  for (size_t i = first; i < tokens.size(); ++i) {

    antlr4::Token *token = tokens.get(i);
    if (token->getType() != antlr4::Token::EOF &&
        token->getChannel() == antlr4::Token::DEFAULT_CHANNEL) {

      return true;

    }

  }

  return false;

}

tree_t *parse_here(const Grammar &grammar, const uint8_t *buf, size_t len,
                   const Limits &limits, parse_status_t &status) {

  uint64_t start = parse_budget_now_ns();

  // UTF-8, and the byte offset of each code point (and of the end)
  std::optional<std::u32string> code_points = antlrcpp::Utf8::strictDecode(
      std::string_view(reinterpret_cast<const char *>(buf), len));
  if (!code_points) {

    status = PARSE_NOT_UTF8;
    return nullptr;

  }

  std::vector<size_t> offsets;
  offsets.reserve(code_points->size() + 1);
  for (size_t i = 0; i < len; ++i) {

    if ((buf[i] & 0xc0) != 0x80) offsets.push_back(i);

  }

  offsets.push_back(len);

  // Lex
  CodePointStream                chars(std::move(*code_points));
  ErrorCounter                   lexer_errors;
  std::unique_ptr<antlr4::Lexer> lexer = grammar.make_lexer(&chars);
  lexer->removeErrorListeners();
  lexer->addErrorListener(&lexer_errors);
  parse_clock::TimedTokenStream tokens(lexer.get());
  tokens.fill();
  if (lexer_errors.errors) {

    status = PARSE_SYNTAX_ERROR;
    return nullptr;

  }

  // Parse
  Guard guard(limits.max_rule_depth);
  if (limits.budget) {

    uint64_t limit = parse_budget_limit_ns(limits.budget, tokens.size());
    if (limit) guard.clock.deadline = start + limit;

  }

  tokens.clock = &guard.clock;
  ErrorCounter                    parser_errors;
  std::unique_ptr<antlr4::Parser> parser = grammar.make_parser(&tokens);
  parser->removeErrorListeners();
  parser->addErrorListener(&parser_errors);
  parser->setErrorHandler(std::make_shared<antlr4::BailErrorStrategy>());
  parser->addParseListener(&guard);
  parse_clock::install_timed_simulator(*parser, guard.clock);
  antlr4::ParserRuleContext *root_ctx = grammar.parse_start(*parser);
  uint64_t                   elapsed = parse_budget_now_ns() - start;
  tokens.clock = nullptr;
  if (parser_errors.errors || has_trailing_tokens(tokens, root_ctx)) {

    status = PARSE_SYNTAX_ERROR;
    return nullptr;

  }

  // Match
  tree_t *tree = detail::match_tree(grammar, *parser, root_ctx, buf, len,
                                    offsets, tokens.size());
  if (!tree) {

    status = PARSE_NO_MATCH;
    return nullptr;

  }

  if (limits.budget) parse_budget_record(limits.budget, elapsed, tokens.size());
  status = PARSE_OK;
  return tree;

}

// The work of one parse thread
struct Job {

  const Grammar *grammar;
  const uint8_t *buf;
  size_t         len;
  const Limits  *limits;
  tree_t        *tree;
  parse_status_t status;

};

void *run_job(void *arg) {

  Job *job = static_cast<Job *>(arg);
  try {

    job->tree = parse_here(*job->grammar, job->buf, job->len, *job->limits,
                           job->status);

  } catch (const antlr4::ParseCancellationException &) {

    // BailErrorStrategy stops at the first syntax error.
    job->status = PARSE_SYNTAX_ERROR;

  } catch (const antlr4::RecognitionException &) {

    job->status = PARSE_SYNTAX_ERROR;

  } catch (...) {

    // StopParse, or a different exception of the runtime or a base class
    job->status = PARSE_LIMIT;

  }

  return nullptr;

}

}  // namespace

tree_t *parse(const Grammar &grammar, const uint8_t *buf, size_t len,
              const Limits &limits, parse_status_t *status) {

  Job            job{&grammar, buf, len, &limits, nullptr, PARSE_LIMIT};
  pthread_attr_t attr;
  pthread_t      thread;
  int            error = pthread_attr_init(&attr);
  if (!error) {

    error = pthread_attr_setstacksize(&attr, limits.stack_size);
    if (!error) error = pthread_create(&thread, &attr, run_job, &job);
    pthread_attr_destroy(&attr);

  }

  if (error) {

    if (status) *status = PARSE_THREAD_ERROR;
    return nullptr;

  }

  pthread_join(thread, nullptr);
  if (status) *status = job.status;
  return job.tree;

}

}  // namespace g4
