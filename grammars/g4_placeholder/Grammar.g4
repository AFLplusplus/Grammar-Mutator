// The parser for .g4 grammars. Parsing test cases with .g4 grammars is not
// supported yet, so this parser rejects all input. The mutator then uses only
// the trees from grammar_generator and from its own mutations.
//
// The first child of "entry" is always a terminal (EOF or an error token), and
// the shim rejects a tree with a terminal root. The lexer rule ANY makes ANTLR
// generate GrammarLexer, which the shim needs.
grammar Grammar;

entry
    : EOF
    ;

ANY
    : .
    ;
