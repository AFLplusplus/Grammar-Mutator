# ANTLR 4 meta-grammar

These files let `grammars/g4_to_json.py` read ANTLR 4 grammar (`.g4`) files.
The build generates a Python parser from them with the ANTLR jar.

The files come from [grammars-v4](https://github.com/antlr/grammars-v4/tree/master/antlr/antlr4),
commit `d0e21ddc17cdca89887ba05c07ddd754d644059c`. They use the BSD license in
their headers.

- `ANTLRv4Parser.g4` and `LexerAdaptor.py` (from `Python3/`) are not changed.
- `ANTLRv4Lexer.g4`: the two lexer actions use `self.` instead of `this.`, for
  the Python target.
