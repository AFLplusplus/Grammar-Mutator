#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Date    : 2020-08-19 00:17:24
# @Author  : Shengtuo Hu
# @Link    : https://shengtuo.me

import os
import sys
import json
import random


class Sanitize:
    def __init__(self, g):
        self.g = g
        self.entry_keys = self.get_entry_key()

    def to_key(self, k):
        s = k.replace('-', '_')
        s = s.replace('[', 'Osq').replace(']', 'Csq')
        s = s.replace('{', 'Obr').replace('}', 'Cbr')
        s = s.replace('import', 'XimportX')
        s = s.replace('class', 'XclassX')
        s = s.replace('def', 'XdefX')
        return s

    def get_entry_key(self):
        key_seen = {}
        for k in self.g:
            if k not in key_seen:
                key_seen[k] = False
            for rule in self.g[k]:
                for token in rule:
                    if token not in self.g:
                        continue
                    if token not in key_seen:
                        key_seen[token] = True
        entry_keys = [k for k in key_seen if not key_seen[k]]
        return entry_keys


class AntlrG(Sanitize):
    def to_key(self, k):
        return super().to_key(k)[1:-1]

    def esc_token(self, t):
        # these are multi-char tokens
        t = t.replace('\\', '\\\\')
        t = t.replace("'", "\\\'")
        t = t.replace('\n', '\\n')
        t = t.replace('\r', '\\r')
        t = t.replace('\t', '\\t')
        return t

    def rule_to_s(self, rule, grammar):
        return ' '.join(["'%s'" % self.esc_token(t)
                         if t not in grammar else self.to_key(t)
                         for t in rule])

    def translate(self):
        lines = ['grammar Grammar;']
        entries = '\n    | '.join([self.to_key(entry_k) + ' EOF' for entry_k in self.entry_keys])
        lines.append('''\
entry
    : %s
    ;''' % entries)
        for k in self.g:
            rules = self.g[k]
            v = '\n    | '.join([self.rule_to_s(rule, self.g)
                                for rule in rules])
            lines.append('''\
%s
    : %s
    ;''' % (self.to_key(k), v))
        return '\n'.join(lines)


def main(grammar_file_path, root_dir):
    random.seed(0)  # Fixed seed

    with open(grammar_file_path, 'r') as fp:
        grammar = json.load(fp)
    g4 = AntlrG(grammar).translate()
    g4_file_path = os.path.join(
        root_dir, 'Grammar.g4')
    with open(g4_file_path, 'w') as fp:
        print(g4, file=fp)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(sys.argv[0] + ' </path/to/grammar/file> </path/to/output/dir>')
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
