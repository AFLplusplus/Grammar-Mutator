Customizing Grammars
===

The grammar file is specified in JSON and is shown as a collection of key/value pairs.
Each key represents a grammar token that is surrounded by angle brackets, and the corresponding value of a key is a list of grammar rules.
In the following case, `<START>` has two rules.

```json
{
    "<START>": [["RULE1"], ["RULE2"]]
}
```

For each grammar rule, it consist of a list of strings, which can either stand for a concrete string or a grammar token.

```json
["<A>", " likes ", "<B>"]
```

Given an input grammar, the grammar mutator constructs a tree representation for each input test case.
The tree is then turned into a concrete input for the target application.

Besides, we provide a script [nautilus_py_grammar_to_json.py](../grammars/nautilus_py_grammar_to_json.py) that can convert a nautilus grammar file to the JSON grammar format used in this project.
We plan to incorporate more features, such as using regex to specify grammar rule, in the future.

## Sample Grammar

Here is a very simple grammar file:

```json
{
    "<A>": [["I ", "<B>"]],
    "<B>": [["like ", "<C>"]],
    "<C>": [["C"], ["C++"]]
}
```

This simple grammar can generate two strings: "I like C" and "I like C++".

## ANTLR 4 Grammars (.g4)

You can also give one or more ANTLR 4 grammar files (`.g4`) instead of a JSON
grammar, for example a grammar from
[grammars-v4](https://github.com/antlr/grammars-v4). The build converts them
into a JSON grammar with [g4_to_json.py](../grammars/g4_to_json.py).

The `.g4` path needs the Python tools in `requirements.txt`. With make, run
`make deps` once to install them into `.venv`. With CMake, the configure step
installs them into the build folder.

```bash
# One combined grammar
make GRAMMAR_FILE=grammars/g4/JSON.g4

# A lexer grammar and a parser grammar
make GRAMMAR_FILE="grammars/g4/javascript/JavaScriptLexer.g4 grammars/g4/javascript/JavaScriptParser.g4"

# CMake: give a list of full paths
cmake -DGRAMMAR_FILE="/path/JavaScriptLexer.g4;/path/JavaScriptParser.g4" ...
```

- Give all `.g4` files of the grammar. If a grammar uses `import`, also give
  the imported files. The build stops if a rule or a token has no definition.
- `GRAMMAR_START` (CMake: `-DGRAMMAR_START`) is the start rule. The default is
  the first parser rule.
- The default grammar name comes from the last file, without `Parser` or
  `Lexer`. So the files above give `libgrammarmutator-javascript.so`.

The conversion works as follows:

- Each subrule and each `?`, `*`, and `+` becomes a helper rule.
- Each lexer rule becomes a rule over characters. A character set, a `~` set,
  or `.` gives one printable ASCII character, a tab, or a newline. A Unicode
  property such as `\p{L}` gives ASCII characters of the same kind (letters
  for `\p{L}`). A lexer rule that cannot give an ASCII character is removed.
- A space comes after each token of a parser rule, because the lexer usually
  skips whitespace. A token that continues to the end of the line, such as a
  line comment, gets a newline after it.
- Actions, predicates, labels, options, and `EOF` are ignored. So a generated
  test case can break a rule that only a predicate or an action checks.
- The rules that the start rule cannot reach are removed.

Limits of the current version:

- The mutator does not parse test cases with a `.g4` grammar yet. Use
  `grammar_generator-<name>` to make the seeds and their trees (see
  [README.md](../README.md#using-generated-seeds)). The mutator skips a queue
  entry that has no tree, and AFL++ fuzzes that entry with its own mutators.
- A token that only the code of a grammar makes (a `tokens { ... }` entry with
  no lexer rule, such as `INDENT` in a Python grammar) cannot be generated.
  The build removes the alternatives that use it and prints a warning.
- Generated characters are ASCII only.
