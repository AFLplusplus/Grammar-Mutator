# ANTLR 4 grammars

Sample ANTLR 4 (`.g4`) grammars. See
[customizing-grammars.md](../../doc/customizing-grammars.md) for how to build
the grammar mutator with a `.g4` grammar.

- `JSON.g4` comes from
  [grammars-v4](https://github.com/antlr/grammars-v4/blob/master/json/JSON.g4),
  commit `30c6ea0dac4a03f0f8bb454a6d15435c424b6f69`. It is taken from "The
  Definitive ANTLR 4 Reference" by Terence Parr.
- `javascript/JavaScriptLexer.g4` and `javascript/JavaScriptParser.g4` come
  from
  [grammars-v4](https://github.com/antlr/grammars-v4/tree/master/javascript/javascript),
  commit `30c6ea0dac4a03f0f8bb454a6d15435c424b6f69`, with no local changes.
  The MIT license is in the file headers. Build with both files:
  `make GRAMMAR_FILE="grammars/g4/javascript/JavaScriptLexer.g4 grammars/g4/javascript/JavaScriptParser.g4"`.
  The grammar has action code and a base class (`superClass`) for checks such
  as strict mode. Generation ignores them, so not all generated test cases
  are valid JavaScript.

Many more grammars are in [grammars-v4](https://github.com/antlr/grammars-v4).
