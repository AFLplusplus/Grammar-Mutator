#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Date    : 2020-06-19 14:45:19
# @Author  : Shengtuo Hu (h1994st@gmail.com)
# @Version : 1.0
import sys
import functools
import itertools
import random
import os
import subprocess
import string
import statistics
import json
from datetime import datetime
from resource import getrusage as resource_usage, RUSAGE_CHILDREN
from time import time as timestamp

from sh import chmod, rm, mkdir
from fuzzingbook.Timer import Timer
from fuzzingbook.ExpectError import ExpectTimeout

START_TIME = datetime.now()
IS_HTML = False
TX = {}


class Sanitize:
    def __init__(self, g):
        self.g = g

    def to_key(self, k):
        s = k.replace('-', '_')
        s = s.replace('[', 'Osq').replace(']','Csq')
        s = s.replace('{','Obr').replace('}','Cbr')
        s = s.replace('import','XimportX')
        s = s.replace('class', 'XclassX')
        s = s.replace('def', 'XdefX')
        return s

    def to_token(self, t):
        return t

    def split_tokens(self, t, grammar):
        if t in grammar: return [t]
        my_tokens = []
        # these should not matter for performance comparisons,
        # and makes my life simpler
        esc = {'\r': '\r', '\n': '\n',
             '\\': '\\',
             '"':'"',
             "'":"'"}
        for i in t:
            if i in esc:
                my_tokens.append(esc[i])
            else:
                my_tokens.append(i)
        return my_tokens

        return list(t)

    def to_rule(self, rule, grammar):
        tokens = [k for t in rule for k in self.split_tokens(t, grammar)]
        return [self.to_token(t) if t not in grammar else self.to_key(t)
                for t in tokens]

    def translate(self):
        new_grammar = {}
        for k in self.g:
            rules = self.g[k]
            new_grammar[self.to_key(k)] = [self.to_rule(rule, self.g) for rule in rules]
        return new_grammar


class CTrans(Sanitize):
    def split_tokens(self, t, grammar):
        if t in grammar:
            return [t]
        my_tokens = []
        for i in t:
            my_tokens.append(i)
        return my_tokens


class Fuzzer:
    def __init__(self, grammar):
        self.grammar = grammar

    def fuzz(self, key='<start>', max_num=None, max_depth=None):
        raise NotImplemented()


class LimitFuzzer(Fuzzer):
    def symbol_cost(self, grammar, symbol, seen):
        if symbol in self.key_cost: return self.key_cost[symbol]
        if symbol in seen:
            self.key_cost[symbol] = float('inf')
            return float('inf')
        v = min((self.expansion_cost(grammar, rule, seen | {symbol})
                    for rule in grammar.get(symbol, [])), default=0)
        self.key_cost[symbol] = v
        return v

    def expansion_cost(self, grammar, tokens, seen):
        return max((self.symbol_cost(grammar, token, seen)
                    for token in tokens if token in grammar), default=0) + 1


class LimitFuzzer(LimitFuzzer):
    def gen_key(self, key, depth, max_depth):
        if key not in self.grammar: return key
        if depth > max_depth:
            clst = sorted([(self.cost[key][str(rule)], rule) for rule in self.grammar[key]])
            rules = [r for c,r in clst if c == clst[0][0]]
        else:
            rules = self.grammar[key]
        return self.gen_rule(random.choice(rules), depth+1, max_depth)

    def gen_rule(self, rule, depth, max_depth):
        return ''.join(self.gen_key(token, depth, max_depth) for token in rule)

    def fuzz(self, key='<start>', max_depth=10):
        return self.gen_key(key=key, depth=0, max_depth=max_depth)


class LimitFuzzer(LimitFuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        self.key_cost = {}
        self.cost = self.compute_cost(grammar)

    def compute_cost(self, grammar):
        cost = {}
        for k in grammar:
            cost[k] = {}
            for rule in grammar[k]:
                cost[k][str(rule)] = self.expansion_cost(grammar, rule, set())
        return cost


class PooledFuzzer(LimitFuzzer):
    def compute_cost(self, grammar, cost={}):
        return {k:sorted([(self.expansion_cost(grammar, rule, set()), rule)
                          for rule in grammar[k]])
                for k in self.grammar}


class PooledFuzzer(PooledFuzzer):
    def cheap_grammar(self):
        new_grammar = {}
        for k in self.cost:
            crules = self.cost[k]
            min_cost = crules[0][0]
            new_grammar[k] = [r for c,r in crules if c == min_cost]
            assert len(new_grammar[k]) > 0
        return new_grammar


class PooledFuzzer(PooledFuzzer):
    def get_strings_for_key(self, grammar, key='<start>'):
        if key not in grammar: return [key]
        v = sum([self.get_strings_for_rule(grammar, rule)
                 for rule in grammar[key]], [])
        return random.sample(v, min(self.MAX_SAMPLE, len(v)))

    def get_strings_for_rule(self, grammar, rule):
        my_strings_list = [self.get_strings_for_key(grammar, key) for key in rule]
        v = [''.join(l) for l in itertools.product(*my_strings_list)]
        return random.sample(v, min(self.MAX_SAMPLE, len(v)))

    def completion_strings(self):
        # we are being choosy
        return {k:self.get_strings_for_key(self.c_grammar, k)
                for k in self.c_grammar}


class PooledFuzzer(PooledFuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        self.c_grammar = self.cheap_grammar()
        self.MAX_SAMPLE = 255
        self.pool_of_strings = self.completion_strings()
        # reorder our grammar rules by cost.
        for k in self.grammar:
            self.grammar[k] = [r for (i,r) in self.cost[k]]
        self.ordered_grammar = True

    def gen_key(self, key, depth, max_depth):
        if key not in self.grammar: return key
        if depth > max_depth:
            return random.choice(self.pool_of_strings[key])
        return self.gen_rule(random.choice(self.grammar[key]), depth+1, max_depth)


# not clear what is the fastest: + or ''.join
# https://stackoverflow.com/questions/1316887/what-is-the-most-efficient-string-concatenation-method-in-python
class PyCompiledFuzzer(PooledFuzzer):
    def add_indent(self, string, indent):
        return '\n'.join([indent + i for i in string.split('\n')])

    # used for escaping inside strings
    def esc(self, t):
        t = t.replace('\\', '\\\\')
        t = t.replace('\n', '\\n')
        t = t.replace('\r', '\\r')
        t = t.replace('\t', '\\t')
        t = t.replace('\b', '\\b')
        t = t.replace('\v', '\\v')
        t = t.replace('"', '\\"')
        return t

    def esc_char(self, t):
        assert len(t) == 1
        t = t.replace('\\', '\\\\')
        t = t.replace('\n', '\\n')
        t = t.replace('\r', '\\r')
        t = t.replace('\t', '\\t')
        t = t.replace('\b', '\\b')
        t = t.replace('\v', '\\v')
        t = t.replace("'", "\\'")
        t = t.replace('"', '\\"')
        return t

    def k_to_s(self, k): return k[1:-1].replace('-', '_')

    def gen_rule_src(self, rule, key, i):
        res = []
        for token in rule:
            if token in self.grammar:
                res.append('''\
gen_%s(next_depth, max_depth)''' % self.k_to_s(token))
            else:
                res.append('''\
result.append("%s")''' % self.esc(token))
        return '\n'.join(res)

    def string_pool_defs(self):
        result =[]
        for k in self.pool_of_strings:
            result.append('''\
pool_of_%(key)s = %(values)s''' % {
                'key':self.k_to_s(k),
                'values': self.pool_of_strings[k]})
        result.append('''
result = []''')
        return '\n'.join(result)

    def gen_main_src(self):
        result = []
        result.append('''
import random
import sys
def main(args):
    global result
    max_num, max_depth = get_opts(args)
    for i in range(max_num):
        gen_start(0, max_depth)
        print(''.join(result))
        result = []

main(sys.argv)''')
        return '\n'.join(result)

    def gen_alt_src(self, key):
        rules = self.grammar[key]
        result = []
        result.append('''
def gen_%(name)s(depth, max_depth):
    next_depth = depth + 1
    if depth > max_depth:
        result.append(random.choice(pool_of_%(name)s))
        return
    val = random.randrange(%(nrules)s)''' % {
            'name':self.k_to_s(key),
            'nrules':len(rules)})
        for i, rule in enumerate(rules):
            result.append('''\
    if val == %d:
%s
        return''' % (i, self.add_indent(self.gen_rule_src(rule, key, i),'        ')))
        return '\n'.join(result)

    def gen_fuzz_src(self):
        result = []
        result.append(self.string_pool_defs())
        for key in self.grammar:
            result.append(self.gen_alt_src(key))
        return '\n'.join(result)

    def fuzz_src(self, key='<start>'):
        result = [self.gen_fuzz_src(),
                  self.gen_main_src()]
        return ''.join(result)


class PyRecCompiledFuzzer(PyCompiledFuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        assert self.ordered_grammar
        self.rec_cost = {}
        self.compute_rule_recursion()

    def kr_to_s(self, key, i): return 'gen_%s_%d' % (self.k_to_s(key), i)
    # the grammar needs to be ordered by the cost.
    # else the ordering will change at the end.

    def is_rule_recursive(self, rname, rule, seen):
        if not rule: return False
        if rname in seen:
            return False # reached another recursive rule without seeing this one
        for token in rule:
            if token not in self.grammar: continue
            for i,trule in enumerate(self.grammar[token]):
                rn = self.kr_to_s(token, i)
                if rn  == rname: return True
                if rn in seen: return False
                v = self.is_rule_recursive(rname, trule, seen | {rn})
                if v: return True
        return False

    def is_key_recursive(self, check, key, seen):
        if not key in self.grammar: return False
        if key in seen: return False
        for rule in self.grammar[key]:
            for token in rule:
                if token not in self.grammar: continue
                if token == check: return True
                v = self.is_key_recursive(check, token, seen | {token})
                if v: return True
        return False

    def compute_rule_recursion(self):
        if IS_HTML:   # TODO -- to much time -- only for HTML
            self.rule_recursion = HTML_RULE_RECURSION
            self.key_recursion = HTML_KEY_RECURSION
            return
        self.rule_recursion = {}
        for k in self.grammar:
            for i_rule,rule in enumerate(self.grammar[k]):
                n = self.kr_to_s(k, i_rule)
                self.rule_recursion[n] = self.is_rule_recursive(n, rule, set())
        self.key_recursion = {}
        for k in self.grammar:
            self.key_recursion[k] = self.is_key_recursive(k, k, set())


class CFuzzer(PyRecCompiledFuzzer):
    def cheap_chars(self, string):
        # to be embedded within single quotes
        escaped = {'t':'\t', 'n': '\n', "'": "\\'", "\\":"\\\\", 'r': '\r'}
        slst = []
        while string:
            c, *string = string
            if c in {'\\'}:
                c1, *string = string
                slst.append(escaped[c1])
            elif c in {"'"}:
                slst.append("\'")
            else:
                slst.append(c)
        return slst

    def gen_rule_src(self, rule, key, i):
        res = []
        for token in rule:
            print("[h1994st]")
            print(rule)
            print(token)
            if token in self.grammar:
                res.append('gen_%s(depth +1);' % self.k_to_s(token))
            else:
                esc_token_chars = [self.esc_char(c) for c in token]
                esc_token = ''.join(esc_token_chars)
                res.append("out(\"%s\", %d);" % (esc_token, len(esc_token_chars)))
        return '\n        '.join(res)

    def gen_alt_src(self, k):
        rules = self.grammar[k]
        print("rules:")
        print(rules)
        cheap_strings = self.pool_of_strings[k]
        result = ['''
void gen_%(name)s(int depth) {
    if (depth > max_depth) {
        int val = map(%(num_cheap_strings)d);
        const char* str = pool_%(name)s[val];
        const int str_l = pool_l_%(name)s[val];
        out(str, str_l);
        return;
    }

    int val = map(%(nrules)d);
    switch(val) {''' % {'name':self.k_to_s(k), 'nrules':len(rules),
                        'num_cheap_strings': len(cheap_strings),
                       }]
        for i, rule in enumerate(rules):
            result.append('''
    case %d:
        %s
        break;''' % (i, self.gen_rule_src(rule, k, i)))
        result.append('''
    }
}
    ''')
        return '\n'.join(result)

    def string_pool_defs(self):
        result = []
        for k in self.grammar:
            cheap_strings = self.pool_of_strings[k]
            result.append('''
const char* pool_%(k)s[] =  {%(cheap_strings)s};
const int pool_l_%(k)s[] =  {%(cheap_strings_len)s};
        ''' % {'k':self.k_to_s(k),
               'cheap_strings': ', '.join(['"%s"' % self.esc(s) for s in cheap_strings]),
               'cheap_strings_len': ', '.join([str(len(s)) for s in cheap_strings])})
        return '\n'.join(result)


    def fn_fuzz_decs(self):
        result = []
        for k in self.grammar:
            result.append('''void gen_%s(int depth);''' % self.k_to_s(k))
        return '\n'.join(result)

    def fn_map_def(self):
        return '''
int map(int v) {
    return random() % v;
}
 '''
    def fn_out_def(self):
        return '''
void out(const char* str, const int str_l) {
    fprintf(stdout, "%.*s", str_l, str);
}
 '''

    def fuzz_hdefs(self):
        return '''
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
'''

    def fuzz_out_var_defs(self):
        return '''
void out(const char* str, const int str_l);'''

    def fuzz_rand_var_defs(self):
        return '''
int map(int v);'''
    def fuzz_stack_var_defs(self):
        return '''
extern int max_depth;'''

    def fuzz_var_defs(self):
        return '\n'.join([self.fuzz_out_var_defs(), self.fuzz_rand_var_defs(), self.fuzz_stack_var_defs()])

    def fn_main_input_frag(self):
        return '''
    if (argc < 3) {
        printf("%s <seed> <max_num> <max_depth>\\n", argv[0]);
        return 0;
    }
    seed = atoi(argv[1]);
    max_num = atoi(argv[2]);
    max_depth = atoi(argv[3]);'''

    def fn_main_loop_frag(self):
        return '''
    for(int i=0; i < max_num; i++) {
        gen_init__();
    }'''

    def fn_main_def(self):
        result = '''
int main(int argc, char** argv) {
    int seed, max_num;
%(input_frag)s
    //srandom(time(0));
    srandom(seed);
%(loop_frag)s
    return 0;
}''' % {'input_frag':self.fn_main_input_frag(),
        'loop_frag': self.fn_main_loop_frag()}
        return result

    def main_stack_var_defs(self):
        return '''
int max_depth = 0;'''

    def main_init_var_defs(self):
        return '''
void gen_init__();'''

    def main_var_defs(self):
        return '\n'.join([self.main_stack_var_defs(), self.main_init_var_defs()])

    def fuzz_fn_defs(self):
        result = []
        for key in self.grammar:
            result.append(self.gen_alt_src(key))
        return '\n'.join(result)

    def fuzz_entry(self):
        return '''
void gen_init__() {
    gen_start(0);
    out("\\n", 1);
    return;
}'''

    def main_hdefs(self):
        return '''
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
'''

    def gen_main_src(self):
        return '\n'.join([self.main_hdefs(),
                          self.main_var_defs(),
                          self.fn_map_def(),
                          self.fn_out_def(),
                          self.fn_main_def()])

    def gen_fuzz_src(self):
        return '\n'.join([self.fuzz_hdefs(),
                          self.fuzz_var_defs(),
                          self.fn_fuzz_decs(),
                          self.string_pool_defs(),
                          self.fuzz_fn_defs(),
                          self.fuzz_entry()])

    def fuzz_src(self, key='<start>'):
        return self.gen_main_src(), self.gen_fuzz_src()


def main(grammar, name):
    # print(grammar)
    # print('==========')
    # c_grammar = CTrans(grammar).translate()
    # print(c_grammar)
    # print('==========')

    c_grammar = grammar

    main_src, fuzz_src = CFuzzer(c_grammar).fuzz_src()
    with open(name + '_c_fuzz.c', 'w') as f:
        print(fuzz_src, file=f)
    with open(name + '_c_main.c', 'w') as f:
        print(main_src, file=f)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(sys.argv[0] + ' </path/to/grammar/file> <name>')
        sys.exit(1)

    grammar_file_path = sys.argv[1]
    with open(grammar_file_path, 'r') as fp:
        grammar = json.load(fp)
        main(grammar, sys.argv[2])
