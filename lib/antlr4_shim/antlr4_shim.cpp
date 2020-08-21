#include "antlr4_shim.h"

#include <antlr4-runtime.h>
#include <GrammarLexer.h>
#include <GrammarParser.h>

using namespace antlr4;

tree_t *tree_from_buf(const uint8_t *data_buf, size_t data_size) {
  ANTLRInputStream input((const char *)data_buf, data_size);
  GrammarLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  tokens.fill();

  for (auto token : tokens.getTokens()) {
    std::cout << token->toString() << std::endl;
  }

  GrammarParser parser(&tokens);
  antlr4::tree::ParseTree* tree = parser.entry();

  std::cout << tree->toStringTree(&parser) << std::endl << std::endl;

  // TODO: convert `antlr4::tree::ParseTree` to `tree_t`
  return nullptr;
}
