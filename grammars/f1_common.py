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

#
# The original version of this file is borrowed from F1 fuzzer:
# https://github.com/vrthra/F1
#
# We have made lots of changes to this file to satisfy our requirements.
#

class Fuzzer:
    def __init__(self, grammar):
        self.grammar = grammar
        self.grammar_keys = list(self.grammar.keys())


class LimitFuzzer(Fuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        self.key_cost = {}
        self.cost = self.compute_cost(grammar)

    def symbol_cost(self, grammar, symbol):
        if symbol not in grammar:
            return len(symbol)  # terminal node
        if symbol in self.key_cost:
            return self.key_cost[symbol]
        return float("inf")

    def expansion_cost(self, grammar, rule):
        ret = 0
        for token in rule:
            ret += self.symbol_cost(grammar, token)
            if ret == float("inf"):
                return ret
        return ret

    def compute_cost(self, grammar):
        '''
        Compute the minimum cost (number of characters) for each key in the grammar.
        '''
        cost = {}
        changed = True
        while changed:
            changed = False
            _cost = {}
            for k in self.grammar_keys:
                _cost[k] = []
                for rule in grammar[k]:
                    rule_cost = self.expansion_cost(grammar, rule)
                    if rule_cost == float("inf"):
                        continue
                    if k not in self.key_cost:
                        self.key_cost[k] = rule_cost
                    if self.key_cost[k] > rule_cost:
                        self.key_cost[k] = rule_cost

                    _cost[k].append((rule_cost, rule))
            if _cost != cost:
                cost = _cost
                changed = True

        # sort
        for k in self.grammar_keys:
            cost[k] = sorted(cost[k])
        return cost
