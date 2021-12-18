#!/usr/bin/env python
# -*- coding: utf-8 -*-

#
# american fuzzy lop++ - grammar mutator
# --------------------------------------
#
# Written by Shengtuo Hu
#
# Copyright 2020 AFLplusplus Project. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# A grammar-based custom mutator written for GSoC '20.
#

import os
import sys
import json
import random

from f1_common import LimitFuzzer


#
# The original Sanitize and AntlrG class are borrowed from F1 fuzzer:
# https://github.com/vrthra/F1
#
class Sanitize(LimitFuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)

        # reorder our grammar rules by cost.
        for k in self.grammar_keys:
            self.grammar[k] = [r for (i, r) in self.cost[k]]
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
        for k in self.grammar_keys:
            if k not in key_seen:
                key_seen[k] = False
            for rule in self.grammar[k]:
                for token in rule:
                    if token not in self.grammar_keys:
                        continue
                    key_seen[token] = True
        entry_keys = [k for k in key_seen if not key_seen[k]]
        return entry_keys


class AntlrG(Sanitize):
    def to_key(self, k):
        return 'node_%s' % super().to_key(k)[1:-1]

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
        for k in self.grammar_keys:
            rules = self.grammar[k]
            v = '\n    | '.join([self.rule_to_s(rule, self.grammar)
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
