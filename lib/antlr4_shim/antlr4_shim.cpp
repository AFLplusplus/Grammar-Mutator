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

#include "antlr4_shim.h"
#include "f1_c_fuzz.h"

#include <antlr4-runtime.h>
#include <GrammarLexer.h>
#include <GrammarParser.h>

using namespace antlr4;

node_t *node_from_parse_tree(antlr4::tree::ParseTree *t) {
  if (antlrcpp::is<antlr4::tree::ErrorNode *>(t)) {
    // error
    return nullptr;
  }

  node_t *node = nullptr;
  // terminal node
  if (antlrcpp::is<antlr4::tree::TerminalNode *>(t)) {
    auto terminal_node = dynamic_cast<antlr4::tree::TerminalNode *>(t);
    auto terminal_node_text = terminal_node->getText();
    node = node_create(NODE_TERM__);
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
  for (int i = 0; i < node->subnode_count; ++i) {
    auto &child = t->children[i];
    subnode = node_from_parse_tree(child);
    node->subnodes[i] = subnode;
  }

  return node;
}

tree_t *tree_from_buf(const uint8_t *data_buf, size_t data_size) {
  ANTLRInputStream input((const char *)data_buf, data_size);
  GrammarLexer     lexer(&input);
  // Disable lexer error output
  lexer.removeErrorListener(&ConsoleErrorListener::INSTANCE);

  CommonTokenStream tokens(&lexer);
  tokens.fill();

  GrammarParser parser(&tokens);
  // Disable parser error output
  parser.removeErrorListener(&ConsoleErrorListener::INSTANCE);

  antlr4::tree::ParseTree *parse_tree = parser.entry();

  node_t *root = node_from_parse_tree(parse_tree->children[0]);
  if (!root) return nullptr;  // parse error

  tree_t *tree = tree_create();
  tree->root = root;
  return tree;
}
