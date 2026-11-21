/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The tests of the C++ matcher (lib/antlr4_shim/g4_matcher*.cpp). Each test
   grammar in tests/g4/grammars/<name> has its tables and its parser in the
   namespace t_<name>. Build and run the tests with
   tests/g4/run_matcher_tests.sh.
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <new>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <unistd.h>

#include "g4_matcher.h"
#include "tree.h"

// The number of operator new calls, and the call that throws std::bad_alloc
// (0: no call throws). The test allocation_failure sets them.
static std::atomic<size_t> new_calls{0};
static std::atomic<size_t> new_fail_at{0};

void *operator new(std::size_t size) {

  if (++new_calls == new_fail_at.load()) throw std::bad_alloc();
  if (void *memory = std::malloc(size ? size : 1)) return memory;
  throw std::bad_alloc();

}

void operator delete(void *memory) noexcept {

  std::free(memory);

}

void operator delete(void *memory, std::size_t) noexcept {

  std::free(memory);

}

#define DECLARE_GRAMMAR(name)         \
  namespace t_##name {                \
                                      \
                                      \
    extern const g4::Grammar grammar; \
                                      \
  }

DECLARE_GRAMMAR(json)
DECLARE_GRAMMAR(lead)
DECLARE_GRAMMAR(nonascii)
DECLARE_GRAMMAR(empty)
DECLARE_GRAMMAR(order)
DECLARE_GRAMMAR(optional)
DECLARE_GRAMMAR(token)
DECLARE_GRAMMAR(split)
DECLARE_GRAMMAR(typecmd)
DECLARE_GRAMMAR(tokenset)
DECLARE_GRAMMAR(leftrec)
DECLARE_GRAMMAR(list)
DECLARE_GRAMMAR(nest)
DECLARE_GRAMMAR(trailing)
DECLARE_GRAMMAR(nomatch)
DECLARE_GRAMMAR(reserved)
DECLARE_GRAMMAR(superclass)
DECLARE_GRAMMAR(spin)
DECLARE_GRAMMAR(handmade)
DECLARE_GRAMMAR(bom)

// The tree code in src/tree.c calls tree_parse. Here it parses with the JSON
// test grammar.
extern "C" tree_t *tree_parse(const uint8_t *buf, size_t len,
                              parse_budget_t *budget, parse_status_t *status) {

  g4::Limits limits;
  limits.budget = budget;
  return g4::parse(t_json::grammar, buf, len, limits, status);

}

namespace {

// The number of failed checks in the current test
int failed_checks = 0;

#define CHECK(cond)                                                         \
  do {                                                                      \
                                                                            \
    if (!(cond)) {                                                          \
                                                                            \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                   #cond);                                                  \
      ++failed_checks;                                                      \
                                                                            \
    }                                                                       \
                                                                            \
  } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

const char *status_name(parse_status_t status) {

  switch (status) {

    case PARSE_OK:
      return "PARSE_OK";
    case PARSE_NOT_UTF8:
      return "PARSE_NOT_UTF8";
    case PARSE_SYNTAX_ERROR:
      return "PARSE_SYNTAX_ERROR";
    case PARSE_LIMIT:
      return "PARSE_LIMIT";
    case PARSE_NO_MATCH:
      return "PARSE_NO_MATCH";
    case PARSE_THREAD_ERROR:
      return "PARSE_THREAD_ERROR";

  }

  return "(unknown status)";

}

#define CHECK_STATUS(result, expected)                                  \
  do {                                                                  \
                                                                        \
    if ((result).status != (expected)) {                                \
                                                                        \
      std::fprintf(stderr, "%s:%d: status %s, expected %s\n", __FILE__, \
                   __LINE__, status_name((result).status),              \
                   status_name(expected));                              \
      ++failed_checks;                                                  \
                                                                        \
    }                                                                   \
                                                                        \
  } while (0)

struct TreeDeleter {

  void operator()(tree_t *tree) const {

    tree_free(tree);

  }

};

using Tree = std::unique_ptr<tree_t, TreeDeleter>;

// The result of one parse
struct Result {

  Tree           tree;
  parse_status_t status;

};

Result parse(const g4::Grammar &grammar, std::string_view data,
             const g4::Limits &limits = g4::Limits()) {

  // A status that parse() never gives, to see that it sets the status
  parse_status_t status = static_cast<parse_status_t>(-1);
  tree_t        *tree =
      g4::parse(grammar, reinterpret_cast<const uint8_t *>(data.data()),
                data.size(), limits, &status);
  return Result{Tree(tree), status};

}

// The nodes with an id, in pre-order
std::vector<node_t *> find(node_t *root, uint32_t id) {

  std::vector<node_t *> found;
  std::vector<node_t *> stack{root};
  while (!stack.empty()) {

    node_t *node = stack.back();
    stack.pop_back();
    if (node->id == id) found.push_back(node);
    for (uint32_t i = node->subnode_count; i > 0; --i) {

      stack.push_back(node->subnodes[i - 1]);

    }

  }

  return found;

}

std::string value(const node_t *node) {

  return std::string(reinterpret_cast<const char *>(node->val_buf),
                     node->val_len);

}

std::vector<std::string> leaves(node_t *root) {

  std::vector<std::string> values;
  for (node_t *node : find(root, 0)) {

    values.push_back(value(node));

  }

  return values;

}

std::string text(node_t *root) {

  std::string joined;
  for (const std::string &leaf : leaves(root)) {

    joined += leaf;

  }

  return joined;

}

// The number of nodes on the longest path from root to a leaf
size_t depth(node_t *root) {

  size_t                                   deepest = 0;
  std::vector<std::pair<node_t *, size_t>> stack{{root, 1}};
  while (!stack.empty()) {

    auto [node, level] = stack.back();
    stack.pop_back();
    if (level > deepest) deepest = level;
    for (uint32_t i = 0; i < node->subnode_count; ++i) {

      stack.push_back({node->subnodes[i], level + 1});

    }

  }

  return deepest;

}

// The index of a key in the tables. Its node id is the index + 1.
uint32_t key_index(const g4::Grammar &grammar, std::string_view name) {

  for (uint32_t i = 0; i < grammar.key_count; ++i) {

    if (grammar.keys[i].name == name) return i;

  }

  std::fprintf(stderr, "no key %.*s\n", static_cast<int>(name.size()),
               name.data());
  std::abort();

}

uint32_t node_id(const g4::Grammar &grammar, std::string_view name) {

  return key_index(grammar, name) + 1;

}

// Parse data, and check that the tree gives data again
Tree round_trip(const g4::Grammar &grammar, std::string_view data) {

  Result result = parse(grammar, data);
  CHECK_STATUS(result, PARSE_OK);
  if (!result.tree) return nullptr;
  CHECK(result.tree->root != nullptr);
  CHECK_EQ(text(result.tree->root), std::string(data));
  return std::move(result.tree);

}

void check_status(const g4::Grammar &grammar, std::string_view data,
                  parse_status_t expected) {

  Result result = parse(grammar, data);
  CHECK_STATUS(result, expected);
  CHECK(!result.tree);

}

// Check that a parse gives a valid status, and the exact text for a tree
void check_any_status(const g4::Grammar &grammar, std::string_view data) {

  Result result = parse(grammar, data);
  switch (result.status) {

    case PARSE_OK:
      CHECK(result.tree != nullptr);
      if (result.tree) CHECK_EQ(text(result.tree->root), std::string(data));
      break;
    case PARSE_NOT_UTF8:
    case PARSE_SYNTAX_ERROR:
    case PARSE_LIMIT:
    case PARSE_NO_MATCH:
      CHECK(!result.tree);
      break;
    default:
      CHECK(!"a valid status");

  }

}

std::string repeat(std::string_view piece, size_t count) {

  std::string out;
  out.reserve(piece.size() * count);
  for (size_t i = 0; i < count; ++i) {

    out += piece;

  }

  return out;

}

// n levels of "(" around "x"
std::string nest_input(size_t n) {

  return repeat("(", n) + "x" + repeat(")", n);

}

const std::string_view JSON_INPUT =
    "{ \"a\" : [1, 2.5e3, true, null],\r\n\t\"b\": {\"\xc3\xa9\": "
    "\"\xc3\xbc\"} }  ";

// -- exact text --------------------------------------------------------------

void json_round_trip() {

  Tree tree = round_trip(t_json::grammar, JSON_INPUT);
  if (!tree) return;
  tree_serialize(tree.get());
  Tree copy(tree_deserialize(tree->ser_buf, tree->ser_len));
  CHECK(copy != nullptr);
  if (copy) CHECK_EQ(text(copy->root), std::string(JSON_INPUT));

}

void bom_is_rejected_not_changed() {

  check_status(t_json::grammar, "\xef\xbb\xbf" + std::string(JSON_INPUT),
               PARSE_SYNTAX_ERROR);

}

void bom_on_a_hidden_channel() {

  // The lexer puts U+FEFF on the hidden channel. The input stream keeps a BOM
  // at the input start, so the tree keeps it too.
  const std::string bom = "\xef\xbb\xbf";
  Tree              tree = round_trip(t_bom::grammar, bom + "ab cd");
  if (tree) {

    CHECK(tree->root->subnode_count > 0);
    CHECK_EQ(value(tree->root->subnodes[0]), bom);

  }

  tree = round_trip(t_bom::grammar, bom);
  if (tree) CHECK((leaves(tree->root) == std::vector<std::string>{bom}));
  round_trip(t_bom::grammar, "ab" + bom + " cd" + bom + bom);

}

void leading_text_goes_to_the_lead_leaf() {

  Tree tree = round_trip(t_lead::grammar, "  // c\nx  y\n");
  if (!tree) return;
  CHECK(tree->root->subnode_count > 0);
  CHECK_EQ(value(tree->root->subnodes[0]), "  // c\n");
  CHECK((leaves(tree->root) ==
         std::vector<std::string>{"  // c\n", "x", "  ", "y", "\n"}));

}

void non_ascii_in_tokens_and_comments() {

  round_trip(t_nonascii::grammar,
             "x = \"\xe6\x97\xa5\xe6\x9c\xac\";\n/* \xc3\xbc */ y = \"a\";");

}

void empty_input() {

  for (std::string_view data : {"", "  \n"}) {

    Tree tree = round_trip(t_empty::grammar, data);
    if (!tree) continue;
    CHECK(tree->root->subnode_count > 0);
    CHECK_EQ(tree->root->subnodes[0]->id, 0u);
    CHECK_EQ(value(tree->root->subnodes[0]), std::string(data));

  }

}

// -- node ids and alternatives -----------------------------------------------

void rule_id_is_the_f1_order() {

  const g4::Grammar &grammar = t_order::grammar;
  Tree               tree = round_trip(grammar, "a");
  if (!tree) return;
  std::vector<node_t *> s = find(tree->root, node_id(grammar, "<s>"));
  CHECK_EQ(s.size(), 1u);
  if (s.size() != 1) return;
  CHECK_EQ(s[0]->rule_id, 0u);
  // The alternative 'a' (without 'b') is first in F1 order.
  const g4::Alternative &first =
      grammar.keys[key_index(grammar, "<s>")].alternatives[0];
  std::string literal;
  for (uint32_t i = 0; i < first.symbol_count; ++i) {

    if (first.symbols[i].kind == g4::Kind::Literal ||
        first.symbols[i].kind == g4::Kind::LiteralCont) {

      literal += first.symbols[i].text;

    }

  }

  CHECK_EQ(literal, "a");

}

void helpers_take_the_f1_first_alternative() {

  // The empty alternative of ID? is first in F1 order. So the first ID?
  // stays empty, and the second one takes the token.
  const g4::Grammar &grammar = t_optional::grammar;
  Tree               tree = round_trip(grammar, "a");
  if (!tree) return;
  std::vector<node_t *> first = find(tree->root, node_id(grammar, "<s__1>"));
  std::vector<node_t *> second = find(tree->root, node_id(grammar, "<s__2>"));
  CHECK_EQ(first.size(), 1u);
  CHECK_EQ(second.size(), 1u);
  if (first.size() != 1 || second.size() != 1) return;
  CHECK_EQ(
      grammar.keys[key_index(grammar, "<s__1>")].alternatives[0].symbol_count,
      0u);
  CHECK_EQ(first[0]->rule_id, 0u);
  CHECK_EQ(first[0]->subnode_count, 0u);
  CHECK((leaves(second[0]) == std::vector<std::string>{"a", ""}));

}

void token_node_has_unknown_rule_id() {

  const g4::Grammar &grammar = t_token::grammar;
  Tree               tree = round_trip(grammar, "hello");
  if (!tree) return;
  std::vector<node_t *> token = find(tree->root, node_id(grammar, "<ID>"));
  CHECK_EQ(token.size(), 1u);
  if (token.size() != 1) return;
  CHECK_EQ(token[0]->rule_id, RULE_ID_UNKNOWN);
  CHECK_EQ(token[0]->subnode_count, 1u);
  if (token[0]->subnode_count != 1) return;
  CHECK_EQ(token[0]->subnodes[0]->id, 0u);
  CHECK_EQ(value(token[0]->subnodes[0]), "hello");

}

void split_literal() {

  Tree tree = round_trip(t_split::grammar, "<a> b ");
  if (!tree) return;
  CHECK((leaves(tree->root) ==
         std::vector<std::string>{"", "<", "a>", " ", "b", " "}));

}

void type_command() {

  // Y has one producer (A); Z has two (B and C), so it gets a Z__type key.
  const g4::Grammar &grammar = t_typecmd::grammar;
  Tree               tree = round_trip(grammar, "a b c");
  if (!tree) return;
  std::vector<std::vector<std::string>> a, z;
  for (node_t *node : find(tree->root, node_id(grammar, "<A>"))) {

    a.push_back(leaves(node));

  }

  for (node_t *node : find(tree->root, node_id(grammar, "<Z__type>"))) {

    z.push_back(leaves(node));

  }

  CHECK((a == std::vector<std::vector<std::string>>{{"a"}}));
  CHECK((z == std::vector<std::vector<std::string>>{{"b"}, {"c"}}));

}

void token_set() {

  round_trip(t_tokenset::grammar, "a b c");
  round_trip(t_tokenset::grammar, "a c a");

}

void left_recursion() {

  const g4::Grammar &grammar = t_leftrec::grammar;
  Tree               tree = round_trip(grammar, "1 + 2+3");
  if (!tree) return;
  CHECK_EQ(find(tree->root, node_id(grammar, "<e>")).size(), 5u);

}

// -- size and depth ----------------------------------------------------------

void long_list_has_no_depth_limit() {

  std::string data = repeat("abc ", 99999) + "abc";
  Tree        tree = round_trip(t_list::grammar, data);
  if (!tree) return;
  CHECK(depth(tree->root) > 100000);

}

// -- skips -------------------------------------------------------------------

void trailing_tokens_are_a_syntax_error() {

  round_trip(t_trailing::grammar, "a ");
  check_status(t_trailing::grammar, "a b c", PARSE_SYNTAX_ERROR);

}

void syntax_error() {

  check_status(t_json::grammar, "{\"a\": }", PARSE_SYNTAX_ERROR);
  check_status(t_json::grammar, "{\"a\": 1} #", PARSE_SYNTAX_ERROR);

}

void lexer_error() {

  // The parser accepts the tokens without "@", but the lexer error stops it.
  check_status(t_json::grammar, "[1, @2]", PARSE_SYNTAX_ERROR);

}

void not_utf8() {

  check_status(t_json::grammar, "\xff", PARSE_NOT_UTF8);
  // A UTF-16 surrogate and an overlong form are not UTF-8 either.
  check_status(t_json::grammar, "[\"\xed\xa0\x80\"]", PARSE_NOT_UTF8);
  check_status(t_json::grammar, "[\"\xc0\xaf\"]", PARSE_NOT_UTF8);

}

void no_match() {

  // The converter removes the alternative with a non-ASCII literal.
  round_trip(t_nomatch::grammar, "x");
  check_status(t_nomatch::grammar, "\xc3\xa9", PARSE_NO_MATCH);

}

void nul_bytes() {

  std::string data("[\"a\0b\"]", 7);
  Result      result = parse(t_json::grammar, data);
  if (result.status == PARSE_OK) {

    CHECK(result.tree != nullptr);
    if (result.tree) CHECK_EQ(text(result.tree->root), data);

  } else {

    CHECK_STATUS(result, PARSE_SYNTAX_ERROR);

  }

}

void reserved_start_rule() {

  round_trip(t_reserved::grammar, "abc");

}

void base_class_exception() {

  check_status(t_superclass::grammar, "tx", PARSE_LIMIT);
  round_trip(t_superclass::grammar, "ax");

}

// -- guards ------------------------------------------------------------------

void depth_limit() {

  check_status(t_nest::grammar, nest_input(200001), PARSE_LIMIT);

  g4::Limits limits;
  limits.max_rule_depth = 1000;
  Result result = parse(t_nest::grammar, nest_input(999), limits);
  CHECK_STATUS(result, PARSE_OK);
  if (result.tree) CHECK_EQ(text(result.tree->root), nest_input(999));
  result = parse(t_nest::grammar, nest_input(1001), limits);
  CHECK_STATUS(result, PARSE_LIMIT);
  CHECK(!result.tree);

}

void thread_error() {

  g4::Limits limits;
  limits.stack_size = SIZE_MAX / 2;
  Result result = parse(t_json::grammar, "[]", limits);
  CHECK_STATUS(result, PARSE_THREAD_ERROR);
  CHECK(!result.tree);

}

struct BudgetDeleter {

  void operator()(parse_budget_t *budget) const {

    parse_budget_free(budget);

  }

};

using Budget = std::unique_ptr<parse_budget_t, BudgetDeleter>;

double seconds_since(std::chrono::steady_clock::time_point start) {

  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
      .count();

}

// "mode x x ... x" with n words "x"
std::string spin_input(std::string_view mode, size_t n) {

  return std::string(mode) + repeat(" x", n);

}

// Parse a "spin" input of 20,000 words with a budget. Each word calls spin()
// at least once, and spin() waits 250 us of wall-clock time. So without a
// budget, the parse takes at least 5 s on any machine.
void check_spin_budget(std::string_view mode) {

  round_trip(t_spin::grammar, spin_input(mode, 3));

  // After the warm-up at 200 ns per token, the limit is
  // max(50 ms, 50 * 200 ns * 20,002 tokens) = about 200 ms.
  Budget budget(parse_budget_create());
  CHECK(budget != nullptr);
  if (!budget) return;
  for (int i = 0; i < PARSE_BUDGET_WARMUP; ++i) {

    parse_budget_record(budget.get(), 200000, 1000);

  }

  g4::Limits limits;
  limits.budget = budget.get();
  auto   start = std::chrono::steady_clock::now();
  Result result = parse(t_spin::grammar, spin_input(mode, 20000), limits);
  double limited = seconds_since(start);
  CHECK_STATUS(result, PARSE_LIMIT);
  CHECK(!result.tree);
  // The parse does not stop before the deadline, and it stops long before
  // the 5 s of a full parse. The upper bound is half of a full parse. It
  // leaves time for a slow machine, a stalled CI runner, and Valgrind (a
  // limited parse takes about 1.3 s under Valgrind).
  CHECK(limited >= 0.19);
  CHECK(limited < 2.5);
  std::printf("    %.*s: limited %.3f s\n", static_cast<int>(mode.size()),
              mode.data(), limited);

}

void budget_stops_a_slow_parse() {

  // "rules": each word is a rule "a", so the clock check at each rule entry
  // can stop the parse.
  check_spin_budget("rules");

}

void budget_checks_the_clock_in_prediction() {

  // "loop": the parse enters one rule (s) at the start, long before the
  // deadline. Then all spin() calls come from one loop in s, and the
  // prediction of each loop step calls spin() (the two alternatives
  // conflict, so the prediction evaluates the predicate). No rule entry
  // follows, so only the clock check in LA() can stop the parse.
  check_spin_budget("loop");

}

void tokens_outside_the_input() {

  // The rule: the tokens must be in input order, must not overlap, and must
  // be in the input. Else the matcher gives PARSE_NO_MATCH. The lexer base
  // class gives a token at the positions 0 to 0 before the real tokens. For
  // an empty input, the token ends after the input. For "ab", it overlaps the
  // real token "ab" (a token before the end of the token before it, so also
  // a token out of order).
  check_status(t_handmade::grammar, "", PARSE_NO_MATCH);
  check_status(t_handmade::grammar, "ab", PARSE_NO_MATCH);
  // For " ab", the made token covers the space, before "ab". The leaves take
  // their bytes from the input, not from the token text "x". So the tree
  // gives the input.
  Tree tree = round_trip(t_handmade::grammar, " ab");
  if (tree) {

    CHECK((leaves(tree->root) ==
           std::vector<std::string>{"", " ", "", "ab", ""}));

  }

}

void budget_records_success() {

  Budget budget(parse_budget_create());
  CHECK(budget != nullptr);
  if (!budget) return;
  for (int i = 0; i < PARSE_BUDGET_WARMUP - 1; ++i) {

    parse_budget_record(budget.get(), 1000, 1000);

  }

  CHECK_EQ(parse_budget_limit_ns(budget.get(), 1000), 0u);
  g4::Limits limits;
  limits.budget = budget.get();
  Result result = parse(t_json::grammar, "[1, 2]", limits);
  CHECK_STATUS(result, PARSE_OK);
  // The parse is the last record of the warm-up.
  CHECK(parse_budget_limit_ns(budget.get(), 1000) > 0);

  // A failed parse does not add a record.
  Budget other(parse_budget_create());
  CHECK(other != nullptr);
  if (!other) return;
  for (int i = 0; i < PARSE_BUDGET_WARMUP - 1; ++i) {

    parse_budget_record(other.get(), 1000, 1000);

  }

  limits.budget = other.get();
  result = parse(t_json::grammar, "[1, ]", limits);
  CHECK_STATUS(result, PARSE_SYNTAX_ERROR);
  CHECK_EQ(parse_budget_limit_ns(other.get(), 1000), 0u);

}

void random_bytes() {

  std::mt19937                       random(12345);
  std::uniform_int_distribution<int> length(0, 64), byte(0, 255);
  for (int i = 0; i < 2000; ++i) {

    std::string data(static_cast<size_t>(length(random)), '\0');
    for (char &c : data) {

      c = static_cast<char>(byte(random));

    }

    check_any_status(t_json::grammar, data);

  }

  size_t files = 0;
  for (const auto &entry :
       std::filesystem::directory_iterator("tests/g4/corpus/json")) {

    std::ifstream in(entry.path(), std::ios::binary);
    std::string   data((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
    for (size_t n = 0; n <= data.size(); ++n) {

      check_any_status(t_json::grammar, std::string_view(data).substr(0, n));

    }

    round_trip(t_json::grammar, data);
    ++files;

  }

  CHECK(files > 0);

}

// -- tables and parser -------------------------------------------------------

// Run a function, and return the text that it writes to stderr
template <typename Function>
std::string stderr_of(Function function) {

  std::fflush(stderr);
  std::FILE *capture = std::tmpfile();
  int        saved = dup(fileno(stderr));
  CHECK(capture != nullptr);
  CHECK(saved >= 0);
  if (!capture || saved < 0) return "";
  dup2(fileno(capture), fileno(stderr));
  function();
  std::fflush(stderr);
  dup2(saved, fileno(stderr));
  close(saved);
  std::string written;
  std::rewind(capture);
  for (int c; (c = std::fgetc(capture)) != EOF;) {

    written += static_cast<char>(c);

  }

  std::fclose(capture);
  return written;

}

size_t count_of(const std::string &text, std::string_view part) {

  size_t count = 0;
  for (size_t pos = text.find(part); pos != std::string::npos;
       pos = text.find(part, pos + 1)) {

    ++count;

  }

  return count;

}

// A copy of the JSON test grammar. With wrong_names, one token type name and
// one rule name of the tables are not in the ANTLR parser.
struct GrammarCopy {

  std::vector<g4::Key>        keys;
  std::vector<g4::ParserRule> rules;
  g4::Grammar                 grammar;

  explicit GrammarCopy(bool wrong_names)
      : keys(t_json::grammar.keys,
             t_json::grammar.keys + t_json::grammar.key_count),
        rules(t_json::grammar.parser_rules,
              t_json::grammar.parser_rules + t_json::grammar.parser_rule_count),
        grammar(t_json::grammar) {

    if (wrong_names) {

      for (g4::Key &key : keys) {

        if (key.token_type == "STRING") key.token_type = "NO_SUCH_TOKEN";

      }

      for (g4::ParserRule &rule : rules) {

        if (rule.name == "pair") rule.name = "no_such_rule";

      }

    }

    grammar.keys = keys.data();
    grammar.parser_rules = rules.data();

  }

};

void tables_without_parser_names() {

  // The names stay wrong for the life of the program, so the copies are
  // static. The matcher keeps the names of each grammar address.
  static const GrammarCopy wrong(true), right(false);
  std::string              written = stderr_of([] {

    check_status(wrong.grammar, JSON_INPUT, PARSE_NO_MATCH);
    check_status(wrong.grammar, "[\"a\"]", PARSE_NO_MATCH);

  });

  // One message for the grammar, not one for each parse
  CHECK_EQ(count_of(written, "do not agree"), 1u);
  CHECK(written.find("token type NO_SUCH_TOKEN") != std::string::npos);
  CHECK(written.find("rule no_such_rule") != std::string::npos);
  CHECK_EQ(stderr_of([] { round_trip(right.grammar, JSON_INPUT); }), "");

}

// -- allocation failures -----------------------------------------------------

// The value of new_calls when the ANTLR parser of counting_json returns
std::atomic<size_t> calls_after_parse{0};

antlr4::ParserRuleContext *parse_and_count(antlr4::Parser &parser) {

  antlr4::ParserRuleContext *root = t_json::grammar.parse_start(parser);
  calls_after_parse = new_calls.load();
  return root;

}

// The JSON test grammar, with a start function that sets calls_after_parse
const g4::Grammar &counting_json() {

  static const g4::Grammar grammar = [] {

    g4::Grammar copy = t_json::grammar;
    copy.parse_start = parse_and_count;
    return copy;

  }();

  return grammar;

}

void allocation_failure() {

  // Each parse makes one operator new call of the matcher throw
  // std::bad_alloc: the call k for each k after the ANTLR parse, up to the
  // number of calls of a full parse. The parse must stop with PARSE_LIMIT,
  // and it must free all nodes. LeakSanitizer or the macOS tool "leaks"
  // finds a node that the matcher does not free. The test does not make the
  // ANTLR runtime fail, because the runtime can leak a parse tree node then.
  const g4::Grammar     &grammar = counting_json();
  const std::string_view data = "{\"a\": [1, {\"b\": null}], \"c\": \"d\"}";
  const uint8_t         *buf = reinterpret_cast<const uint8_t *>(data.data());
  // The first parse fills the DFA cache of the parser. So the next parses
  // make the same allocations.
  round_trip(grammar, data);
  parse_status_t status = PARSE_THREAD_ERROR;
  new_calls = 0;
  Tree full(g4::parse(grammar, buf, data.size(), g4::Limits(), &status));
  const size_t last = new_calls;
  const size_t first = calls_after_parse + 1;
  CHECK_EQ(status, PARSE_OK);
  CHECK(first < last);
  for (size_t k = first; k <= last; ++k) {

    status = PARSE_THREAD_ERROR;
    new_calls = 0;
    new_fail_at = k;
    Tree tree(g4::parse(grammar, buf, data.size(), g4::Limits(), &status));
    new_fail_at = 0;
    CHECK_EQ(status, PARSE_LIMIT);
    CHECK(!tree);

  }

  round_trip(grammar, data);

}

struct Test {

  const char *name;
  void (*run)();

};

const Test TESTS[] = {

    {"json_round_trip", json_round_trip},
    {"bom_is_rejected_not_changed", bom_is_rejected_not_changed},
    {"bom_on_a_hidden_channel", bom_on_a_hidden_channel},
    {"leading_text_goes_to_the_lead_leaf", leading_text_goes_to_the_lead_leaf},
    {"non_ascii_in_tokens_and_comments", non_ascii_in_tokens_and_comments},
    {"empty_input", empty_input},
    {"rule_id_is_the_f1_order", rule_id_is_the_f1_order},
    {"helpers_take_the_f1_first_alternative",
     helpers_take_the_f1_first_alternative},
    {"token_node_has_unknown_rule_id", token_node_has_unknown_rule_id},
    {"split_literal", split_literal},
    {"type_command", type_command},
    {"token_set", token_set},
    {"left_recursion", left_recursion},
    {"long_list_has_no_depth_limit", long_list_has_no_depth_limit},
    {"trailing_tokens_are_a_syntax_error", trailing_tokens_are_a_syntax_error},
    {"syntax_error", syntax_error},
    {"lexer_error", lexer_error},
    {"not_utf8", not_utf8},
    {"no_match", no_match},
    {"nul_bytes", nul_bytes},
    {"reserved_start_rule", reserved_start_rule},
    {"base_class_exception", base_class_exception},
    {"tokens_outside_the_input", tokens_outside_the_input},
    {"depth_limit", depth_limit},
    {"thread_error", thread_error},
    {"budget_stops_a_slow_parse", budget_stops_a_slow_parse},
    {"budget_checks_the_clock_in_prediction",
     budget_checks_the_clock_in_prediction},
    {"budget_records_success", budget_records_success},
    {"random_bytes", random_bytes},
    {"tables_without_parser_names", tables_without_parser_names},
    {"allocation_failure", allocation_failure},

};

}  // namespace

// Run all tests, or only the tests that the arguments name
int main(int argc, char **argv) {

  int failed_tests = 0;
  int count = 0;
  for (const Test &test : TESTS) {

    bool selected = argc == 1;
    for (int i = 1; i < argc; ++i) {

      if (std::strcmp(argv[i], test.name) == 0) selected = true;

    }

    if (!selected) continue;
    ++count;
    failed_checks = 0;
    test.run();
    std::printf("[%s] %s\n", failed_checks ? "FAIL" : " OK ", test.name);
    if (failed_checks) ++failed_tests;

  }

  std::printf("%d tests, %d failed\n", count, failed_tests);
  return failed_tests || count == 0 ? 1 : 0;

}
