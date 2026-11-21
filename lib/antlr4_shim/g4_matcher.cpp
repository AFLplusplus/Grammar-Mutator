/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   The matcher: parse one input with the ANTLR C++ parser of a .g4 grammar,
   and change the parse tree into a tree of the JSON grammar.

   - Each parse tree node of a parser rule gets the key of that rule. Its
     children must match one alternative of the key, through the helper keys.
   - A token gets one leaf with the exact token text. A separator leaf gets
     the text between its token and the next token (whitespace, comments, and
     skipped tokens). The text before the first token goes to the lead leaf.

   So the leaves of the tree give the exact bytes of the input. All leaf text
   comes from the input bytes. The matching and the node building use
   explicit stacks, not recursion. The ANTLR runtime parses with recursion, so
   the parse runs in a thread with a large stack, and a rule-depth limit stops
   a parse that is too deep.
 */

#include <pthread.h>

#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "antlr4-runtime.h"
#include "support/Utf8.h"

#include "g4_matcher.h"

namespace g4 {

namespace {

// The exception that stops a parse at the depth limit or at the deadline. It
// does not derive from antlr4::RecognitionException, so the generated rule
// code does not catch it.
struct StopParse {};

// A token type that no token has
constexpr int64_t NO_TYPE = -2;
// A rule that has no key
constexpr int64_t NO_KEY = -1;

// The token types and rules of the tables as numbers, for one grammar
struct Names {

  // For each key: the token type of a token key, or NO_TYPE
  std::vector<int64_t> token_type;
  // For each parser rule index: its key, or NO_KEY
  std::vector<int64_t> rule_key;

};

// Find the token type names (symbolic names) and the rule names of the tables
// in the parser. Do this once for each grammar.
const Names &names_of(const Grammar &grammar, const antlr4::Parser &parser) {

  static std::mutex                                                  mutex;
  static std::unordered_map<const Grammar *, std::unique_ptr<Names>> cache;

  std::lock_guard<std::mutex> lock(mutex);
  std::unique_ptr<Names>     &names = cache[&grammar];
  if (names) return *names;

  auto                                     made = std::make_unique<Names>();
  const antlr4::dfa::Vocabulary           &vocabulary = parser.getVocabulary();
  std::unordered_map<std::string, int64_t> types;
  for (size_t type = 0; type <= vocabulary.getMaxTokenType(); ++type) {

    std::string name(vocabulary.getSymbolicName(type));
    if (!name.empty()) types.emplace(name, static_cast<int64_t>(type));

  }

  made->token_type.assign(grammar.key_count, NO_TYPE);
  for (uint32_t k = 0; k < grammar.key_count; ++k) {

    const std::string_view name = grammar.keys[k].token_type;
    if (name.empty()) continue;
    auto found = types.find(std::string(name));
    if (found != types.end()) made->token_type[k] = found->second;

  }

  const std::vector<std::string> &rules = parser.getRuleNames();
  made->rule_key.assign(rules.size(), NO_KEY);
  for (uint32_t r = 0; r < grammar.parser_rule_count; ++r) {

    const ParserRule &rule = grammar.parser_rules[r];
    for (size_t index = 0; index < rules.size(); ++index) {

      if (rules[index] == rule.name) made->rule_key[index] = rule.key;

    }

  }

  names = std::move(made);
  return *names;

}

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

  // The deadline (parse_budget_now_ns), or 0 for no deadline
  uint64_t deadline = 0;

  void check_clock() {

    // Do not throw while the stack unwinds for a different exception.
    if (deadline && std::uncaught_exceptions() == 0 &&
        parse_budget_now_ns() > deadline) {

      throw StopParse();

    }

  }

  void enterEveryRule(antlr4::ParserRuleContext *) override {

    if (++depth_ > max_depth_ && std::uncaught_exceptions() == 0) {

      throw StopParse();

    }

    check_clock();

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

// A token stream that reads the clock once every 1,024 LA() calls. The
// prediction code can run a long time inside one rule.
class TimedTokenStream : public antlr4::CommonTokenStream {

 public:
  using antlr4::CommonTokenStream::CommonTokenStream;

  Guard *guard = nullptr;

  size_t LA(ssize_t i) override {

    if (guard && (++calls_ & 1023) == 0) guard->check_clock();
    return antlr4::CommonTokenStream::LA(i);

  }

 private:
  size_t calls_ = 0;

};

// -- matching ----------------------------------------------------------------

// A part of the input: the bytes from begin to end
struct Span {

  size_t begin;
  size_t end;

};

// A child of a parse tree node: a rule node or a visible token
struct Kid {

  // A rule node: its parse tree node. A token: nullptr.
  antlr4::tree::ParseTree *ctx;
  // A rule node: the key of its rule, or NO_KEY
  int64_t key;
  // A token: its token type. A rule node: NO_TYPE.
  int64_t type;
  // A token: its text, and the text after it up to the next visible token
  Span text;
  Span hidden;

};

// The events of a match, which build_nodes changes into nodes
enum class Tag : uint8_t {

  Open,   // start a node of key, with the alternative n (the rule_id)
  Close,  // end n nodes
  Leaf,   // a leaf with the bytes text[0, n)
  Token,  // a token node of key for the child n
  Ctx,    // a rule node for the child n (matched later)

};

struct Event {

  Tag         tag;
  uint32_t    key;
  size_t      n;
  const char *text;

};

// A frame of the match stack: a key, an alternative of it, the index of the
// next symbol, and the number of nodes to end with it
struct Frame {

  uint32_t key;
  uint32_t alternative;
  uint32_t symbol;
  size_t   closes;

};

// A choice point: the frames of the callers, the child position, the helper
// key to enter, and the number of nodes to end with its frame
struct Choice {

  std::vector<Frame> callers;
  size_t             child;
  uint32_t           helper;
  size_t             closes;

};

// The memo key of a choice point: the helper, the child position, and the
// key, alternative, and symbol of each caller frame
using Memo = std::vector<size_t>;

struct MemoHash {

  size_t operator()(const Memo &memo) const {

    size_t hash = memo.size();
    for (size_t value : memo) {

      hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);

    }

    return hash;

  }

};

enum class Run { Fail, Done, Choice };

class Matcher {

 public:
  Matcher(const Grammar &grammar, const Names &names, const uint8_t *buf,
          Span leading)
      : grammar_(grammar), names_(names), buf_(buf), leading_(leading) {

  }

  // Match the children of a parse tree node against the alternatives of key.
  // Return true and keep the events of the first match in F1 order, or
  // return false. The search is depth first, with a stack, not recursion.
  // Each choice point enters a helper key. A choice point that comes back
  // with the same callers at the same position failed before (or is an empty
  // loop), so the search skips it. So each choice point runs at most once per
  // child position.
  bool match(uint32_t key, const std::vector<Kid> &kids) {

    struct Entry {

      Choice   point;
      uint32_t next;  // the next alternative to try
      size_t   mark;  // the number of events before the choice point

    };

    std::unordered_set<Memo, MemoHash> visited;
    std::vector<Entry>                 choices;
    std::vector<Frame>                 stack;
    Choice                             point{{}, 0, key, 1};
    Run                                result = Run::Choice;
    events.clear();
    for (;;) {

      if (result == Run::Choice &&
          grammar_.keys[point.helper].alternative_count > 0) {

        Memo memo{point.helper, point.child};
        for (const Frame &frame : point.callers) {

          memo.insert(memo.end(), {frame.key, frame.alternative, frame.symbol});

        }

        if (visited.insert(std::move(memo)).second) {

          choices.push_back(Entry{std::move(point), 0, events.size()});

        }

      }

      // Try the next alternative of the newest choice point.
      while (!choices.empty() &&
             choices.back().next ==
                 grammar_.keys[choices.back().point.helper].alternative_count) {

        choices.pop_back();

      }

      if (choices.empty()) return false;
      Entry   &entry = choices.back();
      uint32_t alternative = entry.next++;
      events.resize(entry.mark);
      events.push_back(
          Event{Tag::Open, entry.point.helper, alternative, nullptr});
      stack = entry.point.callers;
      stack.push_back(
          Frame{entry.point.helper, alternative, 0, entry.point.closes});
      result = run(stack, entry.point.child, kids, point);
      if (result == Run::Done) return true;

    }

  }

  // The events of the last match
  std::vector<Event> events;

 private:
  // Match symbols from a stack of frames until the next choice point. Return
  // Run::Choice (and set point), Run::Done at the end of a match, or
  // Run::Fail.
  Run run(std::vector<Frame> &stack, size_t i, const std::vector<Kid> &kids,
          Choice &point) {

    while (!stack.empty()) {

      Frame             &frame = stack.back();
      const Alternative &alternative =
          grammar_.keys[frame.key].alternatives[frame.alternative];
      if (frame.symbol == alternative.symbol_count) {

        events.push_back(Event{Tag::Close, 0, frame.closes, nullptr});
        stack.pop_back();
        continue;

      }

      const uint32_t sym = frame.symbol;
      const Symbol  &symbol = alternative.symbols[sym];
      frame.symbol = sym + 1;
      const Kid *kid = i < kids.size() ? &kids[i] : nullptr;
      switch (symbol.kind) {

        case Kind::Token:
          // A token key is one token. Do not look into its alternatives.
          if (!kid || kid->ctx || kid->type != names_.token_type[symbol.key]) {

            return Run::Fail;

          }

          events.push_back(Event{Tag::Token, symbol.key, i, nullptr});
          ++i;
          break;

        case Kind::Rule:
          if (!kid || !kid->ctx || kid->key != symbol.key) return Run::Fail;
          events.push_back(Event{Tag::Ctx, 0, i, nullptr});
          ++i;
          break;

        case Kind::Literal: {

          uint32_t end = sym + 1;
          while (end < alternative.symbol_count &&
                 alternative.symbols[end].kind == Kind::LiteralCont) {

            ++end;

          }

          if (!kid || kid->ctx) return Run::Fail;
          // The token text must be all the pieces joined.
          size_t pos = kid->text.begin;
          for (uint32_t k = sym; k < end; ++k) {

            std::string_view piece = alternative.symbols[k].text;
            if (piece.size() > kid->text.end - pos ||
                (!piece.empty() &&
                 std::memcmp(buf_ + pos, piece.data(), piece.size()) != 0)) {

              return Run::Fail;

            }

            pos += piece.size();

          }

          if (pos != kid->text.end) return Run::Fail;
          for (uint32_t k = sym; k < end; ++k) {

            std::string_view piece = alternative.symbols[k].text;
            events.push_back(Event{Tag::Leaf, 0, piece.size(), piece.data()});

          }

          frame.symbol = end;
          ++i;
          break;

        }

        case Kind::Sep: {

          // The text after the token before the separator
          if (i == 0 || kids[i - 1].ctx) return Run::Fail;
          const Span &hidden = kids[i - 1].hidden;
          events.push_back(
              Event{Tag::Leaf, 0, hidden.end - hidden.begin,
                    reinterpret_cast<const char *>(buf_) + hidden.begin});
          break;

        }

        case Kind::Lead:
          events.push_back(
              Event{Tag::Leaf, 0, leading_.end - leading_.begin,
                    reinterpret_cast<const char *>(buf_) + leading_.begin});
          break;

        case Kind::Helper:
          if (sym + 1 == alternative.symbol_count) {

            // The last symbol: the frame of key ends with the helper.
            size_t closes = frame.closes;
            stack.pop_back();
            point = Choice{stack, i, symbol.key, closes + 1};

          } else {

            point = Choice{stack, i, symbol.key, 1};

          }

          return Run::Choice;

        default:
          return Run::Fail;

      }

    }

    return i == kids.size() ? Run::Done : Run::Fail;

  }

  const Grammar &grammar_;
  const Names   &names_;
  const uint8_t *buf_;
  Span           leading_;

};

// -- node building -----------------------------------------------------------

struct NodeDeleter {

  void operator()(node_t *node) const {

    node_free(node);

  }

};

using NodePtr = std::unique_ptr<node_t, NodeDeleter>;

node_t *checked(node_t *node) {

  if (!node) throw std::bad_alloc();
  return node;

}

// A rule node to match: its key, its parse tree node, and its tree node
struct Todo {

  uint32_t                 key;
  antlr4::tree::ParseTree *ctx;
  node_t                  *node;

};

// A node of build_nodes that is not complete: the node and its children
struct OpenNode {

  node_t               *node;
  std::vector<node_t *> children;

};

void set_children(OpenNode &open) {

  node_init_subnodes(open.node, open.children.size());
  if (!open.children.empty() && !open.node->subnodes) throw std::bad_alloc();
  for (size_t i = 0; i < open.children.size(); ++i) {

    node_set_subnode(open.node, i, open.children[i]);

  }

  open.children.clear();

}

void build_nodes(const std::vector<Event> &events, node_t *target,
                 const std::vector<Kid> &kids, const uint8_t *buf,
                 std::vector<Todo> &todo) {

  std::vector<OpenNode> stack;
  try {

    for (const Event &event : events) {

      switch (event.tag) {

        case Tag::Open: {

          uint32_t rule_id = static_cast<uint32_t>(event.n);
          node_t  *node = target;
          if (stack.empty()) {

            node->rule_id = rule_id;

          } else {

            node = checked(node_create_with_rule_id(event.key + 1, rule_id));
            stack.back().children.push_back(node);

          }

          stack.push_back(OpenNode{node, {}});
          break;

        }

        case Tag::Close:
          for (size_t k = 0; k < event.n; ++k) {

            set_children(stack.back());
            stack.pop_back();

          }

          break;

        case Tag::Leaf:
          stack.back().children.push_back(
              checked(node_create_with_val(0, event.text, event.n)));
          break;

        case Tag::Token: {

          const Span &text = kids[event.n].text;
          NodePtr     token(checked(
              node_create_with_rule_id(event.key + 1, RULE_ID_UNKNOWN)));
          NodePtr leaf(checked(node_create_with_val(0, buf + text.begin,
                                                    text.end - text.begin)));
          node_init_subnodes(token.get(), 1);
          if (!token->subnodes) throw std::bad_alloc();
          node_set_subnode(token.get(), 0, leaf.release());
          stack.back().children.push_back(token.get());
          token.release();
          break;

        }

        case Tag::Ctx: {

          const Kid &kid = kids[event.n];
          node_t    *node =
              checked(node_create(static_cast<uint32_t>(kid.key) + 1));
          stack.back().children.push_back(node);
          todo.push_back(Todo{static_cast<uint32_t>(kid.key), kid.ctx, node});
          break;

        }

      }

    }

  } catch (...) {

    // Free the nodes that are not in the tree yet.
    for (OpenNode &open : stack) {

      for (node_t *child : open.children) {

        node_free(child);

      }

    }

    throw;

  }

}

// Return true if the leaves of root give buf exactly
bool same_bytes(node_t *root, const uint8_t *buf, size_t len) {

  size_t                pos = 0;
  std::vector<node_t *> stack{root};
  while (!stack.empty()) {

    node_t *node = stack.back();
    stack.pop_back();
    if (node->val_len) {

      if (node->val_len > len - pos ||
          std::memcmp(node->val_buf, buf + pos, node->val_len) != 0) {

        return false;

      }

      pos += node->val_len;

    }

    for (uint32_t i = node->subnode_count; i > 0; --i) {

      stack.push_back(node->subnodes[i - 1]);

    }

  }

  return pos == len;

}

// Change the parse tree into a tree. Return nullptr if it does not match.
tree_t *make_tree(const Grammar &grammar, const Names &names,
                  antlr4::ParserRuleContext *root_ctx, const uint8_t *buf,
                  size_t len, const std::vector<size_t> &offsets,
                  size_t token_count) {

  // The visible tokens, in input order. The text between them comes from the
  // input, so it also has the skipped tokens.
  std::vector<antlr4::Token *>           tokens;
  std::vector<antlr4::tree::ParseTree *> walk{root_ctx};
  while (!walk.empty()) {

    antlr4::tree::ParseTree *node = walk.back();
    walk.pop_back();
    if (node->getTreeType() == antlr4::tree::ParseTreeType::RULE) {

      walk.insert(walk.end(), node->children.rbegin(), node->children.rend());

    } else {

      antlr4::Token *token =
          static_cast<antlr4::tree::TerminalNode *>(node)->getSymbol();
      if (token->getType() != antlr4::Token::EOF) tokens.push_back(token);

    }

  }

  // A lexer base class can make tokens with any positions. The tokens must
  // be in input order, must not overlap, and must be in the input.
  const size_t code_points = offsets.size() - 1;
  size_t       previous_end = 0;
  for (antlr4::Token *token : tokens) {

    size_t start = token->getStartIndex();
    size_t end = token->getStopIndex() + 1;  // 0 for an empty token at 0
    if (start < previous_end || end < start || end > code_points) {

      return nullptr;

    }

    previous_end = end;

  }

  Span leading{0, tokens.empty() ? len : offsets[tokens[0]->getStartIndex()]};
  // The text after each token, by token index
  std::vector<Span> hidden(token_count, Span{0, 0});
  for (size_t k = 0; k < tokens.size(); ++k) {

    size_t end =
        k + 1 < tokens.size() ? offsets[tokens[k + 1]->getStartIndex()] : len;
    hidden[tokens[k]->getTokenIndex()] =
        Span{offsets[tokens[k]->getStopIndex() + 1], end};

  }

  auto rule_key = [&](antlr4::tree::ParseTree *ctx) {

    size_t rule = static_cast<antlr4::RuleContext *>(ctx)->getRuleIndex();
    return rule < names.rule_key.size() ? names.rule_key[rule] : NO_KEY;

  };

  NodePtr           root(checked(node_create(grammar.start_key + 1)));
  std::vector<Todo> todo{Todo{grammar.start_key, nullptr, root.get()}};
  std::vector<Kid>  kids;
  Matcher           matcher(grammar, names, buf, leading);
  while (!todo.empty()) {

    Todo item = todo.back();
    todo.pop_back();
    kids.clear();
    if (!item.ctx) {

      // The root: one child, the start rule
      kids.push_back(
          Kid{root_ctx, rule_key(root_ctx), NO_TYPE, {0, 0}, {0, 0}});

    } else {

      for (antlr4::tree::ParseTree *child : item.ctx->children) {

        if (child->getTreeType() == antlr4::tree::ParseTreeType::RULE) {

          kids.push_back(Kid{child, rule_key(child), NO_TYPE, {0, 0}, {0, 0}});
          continue;

        }

        antlr4::Token *token =
            static_cast<antlr4::tree::TerminalNode *>(child)->getSymbol();
        if (token->getType() == antlr4::Token::EOF) continue;
        Span text{offsets[token->getStartIndex()],
                  offsets[token->getStopIndex() + 1]};
        kids.push_back(Kid{nullptr, NO_KEY,
                           static_cast<int64_t>(token->getType()), text,
                           hidden[token->getTokenIndex()]});

      }

    }

    if (!matcher.match(item.key, kids)) return nullptr;
    build_nodes(matcher.events, item.node, kids, buf, todo);

  }

  // The leaves give the input bytes by construction. Check it, so that a
  // tree with other bytes never goes to the fuzzer.
  if (!same_bytes(root.get(), buf, len)) return nullptr;
  tree_t *tree = tree_create();
  if (!tree) throw std::bad_alloc();
  tree->root = root.release();
  return tree;

}

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
  TimedTokenStream tokens(lexer.get());
  tokens.fill();
  if (lexer_errors.errors) {

    status = PARSE_SYNTAX_ERROR;
    return nullptr;

  }

  // Parse
  Guard guard(limits.max_rule_depth);
  if (limits.budget) {

    uint64_t limit = parse_budget_limit_ns(limits.budget, tokens.size());
    if (limit) guard.deadline = start + limit;

  }

  tokens.guard = &guard;
  ErrorCounter                    parser_errors;
  std::unique_ptr<antlr4::Parser> parser = grammar.make_parser(&tokens);
  parser->removeErrorListeners();
  parser->addErrorListener(&parser_errors);
  parser->setErrorHandler(std::make_shared<antlr4::BailErrorStrategy>());
  parser->addParseListener(&guard);
  antlr4::ParserRuleContext *root_ctx = grammar.parse_start(*parser);
  uint64_t                   elapsed = parse_budget_now_ns() - start;
  tokens.guard = nullptr;
  if (parser_errors.errors || has_trailing_tokens(tokens, root_ctx)) {

    status = PARSE_SYNTAX_ERROR;
    return nullptr;

  }

  // Match
  tree_t *tree = make_tree(grammar, names_of(grammar, *parser), root_ctx, buf,
                           len, offsets, tokens.size());
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
