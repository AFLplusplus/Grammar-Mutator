/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The parse tables of a .g4 grammar and the matcher that uses them.

   grammars/g4_c_gen.py writes the tables (g4_grammar.cpp) from the JSON
   grammar and the parse map of g4_to_json.py. The file defines one
   g4::Grammar in the C++ namespace NS of the build (the same namespace as the
   ANTLR -package option). To use it, declare it:

     namespace NS {

     extern const g4::Grammar grammar;

     }

   All tables have static storage and constant initialization. The tables do
   not change, so many threads can read them at the same time.
 */

#ifndef __G4_MATCHER_H__
#define __G4_MATCHER_H__

#include <cstdint>
#include <memory>
#include <string_view>

namespace antlr4 {

class CharStream;
class Lexer;
class Parser;
class ParserRuleContext;
class TokenStream;

}  // namespace antlr4

namespace g4 {

/* The kind of a symbol in an alternative (the "kinds" of the parse map) */
enum class Kind : uint8_t {

  /* A parser rule key. It matches a rule child whose parser rule has this key
     (see ParserRule). */
  Rule,
  /* A token key. It matches one token whose ANTLR token type name is the
     token_type of the key. The script uses this kind for each symbol that is
     a token key, whatever its kind in the parse map. */
  Token,
  /* The first piece of a literal. With the LiteralCont symbols after it, it
     matches one token whose text is all the pieces joined. Each piece gives
     one leaf. */
  Literal,
  /* The next piece of a literal. It always follows a Literal or LiteralCont
     symbol. */
  LiteralCont,
  /* A separator leaf: the input text after the token before it, up to the
     next visible token. It fails if the child before it is not a token. */
  Sep,
  /* The lead leaf: the input text before the first visible token */
  Lead,
  /* A helper key. The matcher enters its alternatives (a choice point). */
  Helper,

};

/* One symbol of an alternative */
struct Symbol {

  Kind kind;
  /* Rule, Token, and Helper: the index of the key in Grammar::keys (the node
     id is the index + 1). The other kinds: 0. */
  uint32_t key;
  /* Literal, LiteralCont, Sep, and Lead: the UTF-8 bytes of the symbol (may
     hold any byte). The other kinds: empty. The matcher does not use the
     text of Sep and Lead. */
  std::string_view text;

};

/* One alternative of a key */
struct Alternative {

  const Symbol *symbols;                    /* nullptr if symbol_count is 0 */
  uint32_t      symbol_count;

};

/* One key of the JSON grammar */
struct Key {

  std::string_view name;             /* the JSON key, for example "<value>" */
  /* The alternatives in F1 order (the order of f1_c_gen.py). The position of
     an alternative is the rule_id of its node. A key that the parse map does
     not describe (a lexer key) has no alternatives: the matcher never enters
     it, so a Helper symbol of such a key fails. */
  const Alternative *alternatives;     /* nullptr if alternative_count is 0 */
  uint32_t           alternative_count;
  /* A token key: its ANTLR token type name. Another key: empty. */
  std::string_view token_type;

};

/* A parser rule of the ANTLR parser and its key. A rule child of the parse
   tree gets the key of its rule. A rule that is not in the list has no key,
   so its child matches no Rule symbol. */
struct ParserRule {

  std::string_view name;     /* the rule name, as in Parser::getRuleNames() */
  uint32_t         key;            /* the index of the key in Grammar::keys */

};

/* The grammar descriptor: the tables and the glue to the ANTLR classes */
struct Grammar {

  /* The keys in JSON order. The node id of keys[i] is i + 1. */
  const Key        *keys;
  uint32_t          key_count;
  const ParserRule *parser_rules;
  uint32_t          parser_rule_count;
  /* The index of the key "<START>". Its one alternative is a Lead symbol and
     the Rule symbol of the start rule key. The root node gets this key. */
  uint32_t start_key;
  /* The name of the ANTLR start rule */
  std::string_view start_rule;
  /* Make the lexer (class NS::<lexer_module>) that reads input. */
  std::unique_ptr<antlr4::Lexer> (*make_lexer)(antlr4::CharStream *input);
  /* Make the parser (class NS::<parser_module>) that reads tokens. */
  std::unique_ptr<antlr4::Parser> (*make_parser)(antlr4::TokenStream *tokens);
  /* Call the start rule method on a parser from make_parser, and return the
     context of the start rule. The C++ target adds "_" to a rule name that
     it reserves (for example "default_"). The tables use the same name. */
  antlr4::ParserRuleContext *(*parse_start)(antlr4::Parser &parser);

};

}  // namespace g4

#endif
