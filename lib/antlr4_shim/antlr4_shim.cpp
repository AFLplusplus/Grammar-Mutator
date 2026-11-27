/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Written by Shengtuo Hu

   Copyright 2020 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

#include <exception>

#include <antlr4-runtime.h>
#include <GrammarLexer.h>
#include <GrammarParser.h>

#include "antlr4_shim.h"
#include "parse_clock.h"

using namespace antlr4;

node_t *node_from_parse_tree(antlr4::tree::ParseTree *t) {
  node_t *node = nullptr;

  if (antlrcpp::is<antlr4::tree::ErrorNode *>(t)) {
    // error - treat the error portion as a terminal node
    // we do not want to lose test case information
    auto terminal_node = dynamic_cast<antlr4::tree::TerminalNode *>(t);
    node = node_create(0);
    // ANTLR can make up a token in recovery (its text is "<missing X>"). Such
    // a token has no start index and is not in the input. Give it no value,
    // so that the tree bytes stay the input bytes.
    auto symbol = terminal_node->getSymbol();
    if (symbol && symbol->getStartIndex() == INVALID_INDEX) return node;
    auto terminal_node_text = terminal_node->getText();
    node_set_val(node, terminal_node_text.c_str(), terminal_node_text.length());
    return node;
  }

  // terminal node
  if (antlrcpp::is<antlr4::tree::TerminalNode *>(t)) {
    auto terminal_node = dynamic_cast<antlr4::tree::TerminalNode *>(t);
    auto terminal_node_text = terminal_node->getText();
    node = node_create(0);
    node_set_val(node, terminal_node_text.c_str(), terminal_node_text.length());
    return node;
  }

  if (!antlrcpp::is<antlr4::ParserRuleContext *>(t)) {
    // error
    return nullptr;
  }

  // non-terminal node
  auto non_terminal_node = dynamic_cast<antlr4::ParserRuleContext *>(t);
  node = node_create_with_rule_id(non_terminal_node->getRuleIndex(),
                                  non_terminal_node->getAltNumber() - 1);
  node_init_subnodes(node, t->children.size());
  node_t *subnode;
  for (uint32_t i = 0; i < node->subnode_count; ++i) {
    auto &child = t->children[i];
    subnode = node_from_parse_tree(child);
    node->subnodes[i] = subnode;
    subnode->parent = node;
  }

  return node;
}

tree_t *tree_parse(const uint8_t *data_buf, size_t data_size,
                   parse_budget_t *budget, parse_status_t *status) {
  node_t  *root;
  uint64_t start = parse_budget_now_ns();
  uint64_t elapsed = 0;
  size_t   token_count = 0;

  // A returned tree means PARSE_OK, even if ANTLR recovered from errors.
  // NULL means PARSE_SYNTAX_ERROR, PARSE_NOT_UTF8 if the input is not UTF-8,
  // or PARSE_LIMIT if the time limit stopped the parse.
  if (status) *status = PARSE_SYNTAX_ERROR;

  /**
   * Use try/catch to handle exceptions from ANTLR4 runtime library. If any
   * errors occur, a nullptr will be returned.
   *
   * A known issue: ANTLRInputStream does not accept string with special
   * character (https://github.com/antlr/antlr4/issues/2036)
   */
  try {
    ANTLRInputStream input((const char *)data_buf, data_size);
    GrammarLexer     lexer(&input);
    // Disable lexer error output
    lexer.removeErrorListener(&ConsoleErrorListener::INSTANCE);

    parse_clock::TimedTokenStream tokens(&lexer);
    tokens.fill();

    // The time limit of the parse (none in the warm-up of the budget)
    parse_clock::Clock clock;
    if (budget) {
      clock.deadline = parse_budget_deadline_ns(
          start, parse_budget_limit_ns(budget, tokens.size()));
    }

    tokens.clock = &clock;

    GrammarParser parser(&tokens);
    // Disable parser error output
    parser.removeErrorListener(&ConsoleErrorListener::INSTANCE);
    parse_clock::ClockListener clock_listener(clock);
    parser.addParseListener(&clock_listener);
    parse_clock::install_timed_simulator(parser, clock);

    antlr4::tree::ParseTree *parse_tree = parser.entry();
    elapsed = parse_budget_now_ns() - start;
    token_count = tokens.size();
    tokens.clock = nullptr;
    if (!parse_tree->children.size()) {
#ifdef DEBUG_BUILD
      fprintf(stderr, "ANTLR4 parsing error: No child nodes\n");
#endif
      return nullptr;
    }

    root = node_from_parse_tree(parse_tree->children[0]);
  } catch (const parse_clock::StopParse &) {
    if (status) *status = PARSE_LIMIT;
    return nullptr;
  } catch (antlr4::IllegalArgumentException &e) {
    // ANTLRInputStream throws this for bytes that are not UTF-8
    if (status) *status = PARSE_NOT_UTF8;
#ifdef DEBUG_BUILD
    fprintf(stderr, "ANTLR4 parsing error: %s\n", e.what());
#endif
    return nullptr;
  } catch (std::exception &e) {
#ifdef DEBUG_BUILD
    fprintf(stderr, "ANTLR4 parsing error: %s\n", e.what());
#endif
    return nullptr;
  }

  if (!root) return nullptr;  // parse error

  // Reject degenerate trees whose root is a terminal/error node (id==0).
  // Such trees have non_term_size==0 and break every mutation that picks a
  // non-terminal node.
  if (root->id == 0) {
    node_free(root);
    return nullptr;
  }

  // Record the speed of a successful parse
  if (budget) parse_budget_record(budget, elapsed, token_count);

  tree_t *tree = tree_create();
  tree->root = root;
  if (status) *status = PARSE_OK;
  return tree;
}

bool tree_parser_cache_states(size_t *parser_states, size_t *lexer_states) {
  if (!parser_states || !lexer_states) return false;

  try {
    // Make a lexer and a parser on an empty input. Their simulators refer to the
    // shared static caches. This changes no cache.
    ANTLRInputStream  input("");
    GrammarLexer      lexer(&input);
    CommonTokenStream tokens(&lexer);
    GrammarParser     parser(&tokens);

    size_t count = 0;
    for (auto &dfa : parser.getInterpreter<atn::ParserATNSimulator>()
                         ->decisionToDFA)
      count += dfa.states.size();
    *parser_states = count;

    count = 0;
    for (auto &dfa : lexer.getInterpreter<atn::LexerATNSimulator>()
                         ->_decisionToDFA)
      count += dfa.states.size();
    *lexer_states = count;
  } catch (...) {
    // No exception may cross the C frames of the caller
    return false;
  }

  return true;
}
