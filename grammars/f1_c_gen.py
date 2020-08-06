#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Date    : 2020-06-19 14:45:19
# @Author  : Shengtuo Hu (h1994st@gmail.com)
# @Version : 1.0
import sys
import itertools
import random
import os
import string
import json
from datetime import datetime

START_TIME = datetime.now()
IS_HTML = False
TX = {}


class TreeNode:
    node_type: int = 0
    val: str = ''
    parent: 'TreeNode' = None
    subnodes: list = None

    def __init__(self, node_type=0, val='', subnodes=None):
        super().__init__()

        self.node_type = node_type
        self.val = val

        if subnodes is None:
            subnodes = []
        self.subnodes = subnodes
        for subnode in self.subnodes:
            subnode.parent = self

    def __len__(self):
        return len(self.subnodes)

    def __setitem__(self, key, value):
        self.subnodes[key] = value

    def __getitem__(self, key):
        return self.subnodes[key]

    def append_subnode(self, subnode: 'TreeNode'):
        subnode.parent = self
        self.subnodes.append(subnode)

    def to_bytes(self):
        ret = bytes()

        # type
        ret += self.node_type.to_bytes(4, byteorder='little', signed=False)
        # subnode_count
        subnode_count = len(self)
        ret += subnode_count.to_bytes(4, byteorder='little', signed=False)
        # val_len
        val_len = len(self.val)
        ret += val_len.to_bytes(4, byteorder='little', signed=False)
        # val
        ret += bytes(self.val, 'utf-8')

        # subnodes
        for subnode in self.subnodes:
            ret += subnode.to_bytes()

        return ret

    @staticmethod
    def from_bytes(data: bytes):
        node = TreeNode()
        consumed = 0

        # type
        node.node_type = int.from_bytes(data[consumed:consumed + 4], byteorder='little', signed=False)
        consumed += 4
        # subnode_count
        subnode_count = int.from_bytes(data[consumed:consumed + 4], byteorder='little', signed=False)
        consumed += 4
        # val_len
        val_len = int.from_bytes(data[consumed:consumed + 4], byteorder='little', signed=False)
        consumed += 4
        # val
        if val_len != 0:
            node.val = data[consumed:consumed + val_len].decode('utf-8')
        consumed += val_len

        # subnodes
        for _ in range(subnode_count):
            subnode, sub_consumed = TreeNode.from_bytes(data[consumed:])

            node.append_subnode(subnode)
            consumed += sub_consumed

        return node, consumed

    def __str__(self):
        ret = ''
        if len(self) == 0:
            if self.val is None or len(self.val) == 0:
                return ret
            ret += self.val
            return ret

        # subnodes
        for subnode in self.subnodes:
            ret += str(subnode)

        return ret


def bytes_to_c_str(data: bytes):
    return ''.join(["\\x%02X" % x for x in data])

class Fuzzer:
    def __init__(self, grammar):
        self.grammar = grammar


class LimitFuzzer(Fuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        self.key_cost = {}
        self.cost = self.compute_cost(grammar)

    def symbol_cost(self, grammar, symbol, seen):
        if symbol in self.key_cost:
            return self.key_cost[symbol]
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

    def gen_key(self, key, depth, max_depth):
        if key not in self.grammar:
            return key
        if depth > max_depth:
            clst = sorted(
                [(self.cost[key][str(rule)], rule)
                 for rule in self.grammar[key]])
            rules = [r for c, r in clst if c == clst[0][0]]
        else:
            rules = self.grammar[key]
        return self.gen_rule(random.choice(rules), depth + 1, max_depth)

    def gen_rule(self, rule, depth, max_depth):
        return ''.join(self.gen_key(token, depth, max_depth) for token in rule)

    def compute_cost(self, grammar):
        cost = {}
        for k in grammar:
            cost[k] = {}
            for rule in grammar[k]:
                cost[k][str(rule)] = self.expansion_cost(grammar, rule, set())
        return cost


class PooledFuzzer(LimitFuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        self.c_grammar = self.cheap_grammar()
        self.MAX_SAMPLE = 255
        self.pool_of_strings = self.completion_strings()

        # reorder our grammar rules by cost.
        for k in self.grammar:
            self.grammar[k] = [r for (i, r) in self.cost[k]]
        self.ordered_grammar = True

        self.grammar_keys = list(self.grammar.keys())
        self.pool_of_trees = self.completion_trees()

    def compute_cost(self, grammar):
        return {k: sorted([(self.expansion_cost(grammar, rule, set()), rule)
                           for rule in grammar[k]])
                for k in self.grammar}

    def cheap_grammar(self):
        new_grammar = {}
        for k in self.cost:
            crules = self.cost[k]
            min_cost = crules[0][0]
            new_grammar[k] = [r for c, r in crules if c == min_cost]
            assert len(new_grammar[k]) > 0
        return new_grammar

    def get_strings_for_key(self, grammar, key='<start>'):
        if key not in grammar:
            return [key]
        v = sum([self.get_strings_for_rule(grammar, rule)
                 for rule in grammar[key]], [])
        return random.sample(v, min(self.MAX_SAMPLE, len(v)))

    def get_strings_for_rule(self, grammar, rule):
        my_strings_list = [
            self.get_strings_for_key(grammar, key) for key in rule]
        v = [''.join(l) for l in itertools.product(*my_strings_list)]
        return random.sample(v, min(self.MAX_SAMPLE, len(v)))

    def completion_strings(self):
        # we are being choosy
        return {k: self.get_strings_for_key(self.c_grammar, k)
                for k in self.c_grammar}

    def k_to_id(self, k):
        return self.grammar_keys.index(k) + 1

    def get_trees_for_key(self, grammar, key='<start>'):
        if key not in grammar:
            return [TreeNode(node_type=0, val=key)]
        v = sum([self.get_trees_for_rule(grammar, key, rule)
                 for rule in grammar[key]], [])
        return random.sample(v, min(self.MAX_SAMPLE, len(v)))

    def get_trees_for_rule(self, grammar, key, rule):
        my_trees_list = [
            self.get_trees_for_key(grammar, key) for key in rule]
        v = [TreeNode(node_type=self.k_to_id(key), subnodes=subnodes)
             for subnodes in itertools.product(*my_trees_list)]
        return random.sample(v, min(self.MAX_SAMPLE, len(v)))

    def completion_trees(self):
        return {k: self.get_trees_for_key(self.c_grammar, k)
                for k in self.c_grammar}

    def gen_key(self, key, depth, max_depth):
        if key not in self.grammar:
            return key
        if depth > max_depth:
            return random.choice(self.pool_of_strings[key])
        return self.gen_rule(
            random.choice(self.grammar[key]), depth + 1, max_depth)


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

    def k_to_s(self, k):
        return k[1:-1].replace('-', '_')


class PyRecCompiledFuzzer(PyCompiledFuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        self.key_recursion = {}
        self.rule_recursion = {}
        assert self.ordered_grammar
        self.rec_cost = {}
        self.compute_rule_recursion()

    def kr_to_s(self, key, i):
        return 'gen_%s_%d' % (self.k_to_s(key), i)

    # the grammar needs to be ordered by the cost.
    # else the ordering will change at the end.

    def is_rule_recursive(self, rname, rule, seen):
        if not rule:
            return False
        if rname in seen:
            # reached another recursive rule without seeing this one
            return False
        for token in rule:
            if token not in self.grammar:
                continue
            for i, trule in enumerate(self.grammar[token]):
                rn = self.kr_to_s(token, i)
                if rn == rname:
                    return True
                if rn in seen:
                    return False
                v = self.is_rule_recursive(rname, trule, seen | {rn})
                if v:
                    return True
        return False

    def is_key_recursive(self, check, key, seen):
        if key not in self.grammar:
            return False
        if key in seen:
            return False
        for rule in self.grammar[key]:
            for token in rule:
                if token not in self.grammar:
                    continue
                if token == check:
                    return True
                v = self.is_key_recursive(check, token, seen | {token})
                if v:
                    return True
        return False

    def compute_rule_recursion(self):
        for k in self.grammar:
            for i_rule, rule in enumerate(self.grammar[k]):
                n = self.kr_to_s(k, i_rule)
                self.rule_recursion[n] = self.is_rule_recursive(n, rule, set())
        for k in self.grammar:
            self.key_recursion[k] = self.is_key_recursive(k, k, set())


class CFuzzer(PyRecCompiledFuzzer):
    def cheap_chars(self, string):
        # to be embedded within single quotes
        escaped = {'t': '\t', 'n': '\n', "'": "\\'", "\\": "\\\\", 'r': '\r'}
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
        ntokens = len(rule)
        res.append(
            'node->subnodes = (node_t**)malloc(%d * sizeof(node_t*));' % (
                ntokens))
        res.append('node->subnode_count = %d;' % ntokens)
        for i, token in enumerate(rule):
            if token in self.grammar:
                res.append('subnode = gen_%s(depth +1);' % self.k_to_s(token))
                res.append('node->non_term_size += 1;')
                if key == token:
                    res.append('node->recursion_edge_size += 1;')
            else:
                esc_token_chars = [self.esc_char(c) for c in token]
                esc_token = ''.join(esc_token_chars)
                res.append(
                    'subnode = node_create_with_val(TERM_NODE, "%s", %d);' % (
                        esc_token, len(esc_token_chars)))
            # res.append('node_append_subnode(node, subnode);')
            res.append('node->subnodes[%d] = subnode;' % i)
        return '\n    '.join(res)

    def gen_alt_src(self, k):
        rules = self.grammar[k]
        cheap_strings = self.pool_of_strings[k]
        result = list()
        result.append('''
node_t *gen_%(name)s(int depth) {
  node_t *node = node_create(%(node_type)s);

  if (depth > max_depth) {
    int val = map_rand(%(num_cheap_strings)d);
    const char* str = pool_%(name)s[val];
    const int str_l = pool_l_%(name)s[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(%(nrules)d);
  node_t *subnode = NULL;
  switch(val) {''' % {
            'node_type': self.k_to_s(k).upper(),
            'name': self.k_to_s(k),
            'nrules': len(rules),
            'num_cheap_strings': len(cheap_strings),
        })

        for i, rule in enumerate(rules):
            result.append('''
  case %d:
    %s
    break;''' % (i, self.gen_rule_src(rule, k, i)))

        result.append('''
  }

  return node;
}''')
        return '\n'.join(result)

    def string_pool_defs(self):
        result = []
        for k in self.grammar:
            cheap_strings = self.pool_of_strings[k]
            result.append('''
const char* pool_%(k)s[] =  {%(cheap_strings)s};
const int pool_l_%(k)s[] =  {%(cheap_strings_len)s};''' % {
                'k': self.k_to_s(k),
                'cheap_strings': ', '.join(['"%s"' % self.esc(s) for s in cheap_strings]),
                'cheap_strings_len': ', '.join([str(len(s)) for s in cheap_strings])})
        return '\n'.join(result)

    def gen_alt_src_ser_tree(self, k):
        rules = self.grammar[k]
        cheap_trees = self.pool_of_trees[k]
        result = list()
        result.append('''
node_t *gen_%(name)s(int depth) {
  node_t *node = NULL;

  if (depth > max_depth) {
    int val = map_rand(%(num_cheap_trees)d);
    size_t consumed = 0;
    const char* ser_data = pool_ser_%(name)s[val];
    const int ser_data_l = pool_l_ser_%(name)s[val];
    node = _node_deserialize(ser_data, ser_data_l, &consumed);
    return node;
  }

  node = node_create(%(node_type)s);

  int val = map_rand(%(nrules)d);
  node_t *subnode = NULL;
  switch(val) {''' % {
            'node_type': self.k_to_s(k).upper(),
            'name': self.k_to_s(k),
            'nrules': len(rules),
            'num_cheap_trees': len(cheap_trees),
        })

        for i, rule in enumerate(rules):
            result.append('''
  case %d:
    %s
    break;''' % (i, self.gen_rule_src(rule, k, i)))

        result.append('''
  }

  return node;
}''')
        return '\n'.join(result)

    def ser_tree_pool_defs(self):
        result = []
        for k in self.grammar:
            cheap_trees = self.pool_of_trees[k]
            ser_cheap_trees = [tree.to_bytes() for tree in cheap_trees]
            ser_cheap_trees_c_str = [bytes_to_c_str(ser_tree) for ser_tree in ser_cheap_trees]
            result.append('''
const char* pool_ser_%(k)s[] = {%(ser_trees)s};
const int pool_l_ser_%(k)s[] = {%(ser_trees_len)s};''' % {
                'k': self.k_to_s(k),
                'ser_trees': ', '.join(['"%s"' % ser_tree_c_str for ser_tree_c_str in ser_cheap_trees_c_str]),
                'ser_trees_len': ', '.join([str(len(ser_tree))for ser_tree in ser_cheap_trees])})
        return '\n'.join(result)

    def fuzz_fn_decs(self):
        result = []
        for k in self.grammar:
            result.append('''node_t *gen_%s(int depth);''' % self.k_to_s(k))
        return '\n'.join(result)

    def fuzz_fn_defs(self):
        result = []
        for key in self.grammar:
            # result.append(self.gen_alt_src(key))
            # NOTE: use trees instead
            result.append(self.gen_alt_src_ser_tree(key))
        return '\n'.join(result)

    def node_type_decs(self):
        result = '''
enum node_type {
  TERM_NODE = 0,
  %s
};'''
        node_types = []
        for k in self.grammar:
            node_types.append('%s,' % self.k_to_s(k).upper())
        return result % ('\n  '.join(node_types))

    def fuzz_fn_array_defs(self):
        result = '''
gen_func_t gen_funcs[%d] = {
  NULL,
  %s
};
'''
        fuzz_fn_names = []
        for k in self.grammar:
            fuzz_fn_names.append('gen_%s,' % self.k_to_s(k))
        return result % (
            len(self.grammar.keys()) + 1,
            '\n  '.join(fuzz_fn_names))

    def gen_fuzz_hdr(self):
        hdr_content = '''
#ifndef __F1_C_FUZZ_H__
#define __F1_C_FUZZ_H__

#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int max_depth;

%(fuzz_fn_decs)s

tree_t *gen_init__();

%(node_type_decs)s

typedef node_t *(*gen_func_t)(int depth);
extern gen_func_t gen_funcs[%(num_nodes)d];

#ifdef __cplusplus
}
#endif

#endif'''

        params = {
            "fuzz_fn_decs": self.fuzz_fn_decs(),
            "node_type_decs": self.node_type_decs(),
            "num_nodes": len(self.grammar.keys()) + 1
        }

        return hdr_content % params

    def gen_fuzz_src(self):
        src_content = '''
#include <stdlib.h>
#include <string.h>

#include "tree.h"
#include "f1_c_fuzz.h"

extern node_t *_node_deserialize(const uint8_t *data_buf, size_t data_size, size_t *consumed_size);

int max_depth = -1;

static inline int map_rand(int v) {
  return random() %% v;
}
%(string_pool_defs)s

%(fuzz_fn_defs)s

%(fuzz_fn_array_defs)s

tree_t *gen_init__() {
  tree_t *tree = tree_create();
  tree->root = gen_funcs[1](0);
  return tree;
}'''

        params = {
            "string_pool_defs": self.string_pool_defs(),
            "ser_tree_pool_defs": self.ser_tree_pool_defs(),
            "fuzz_fn_defs": self.fuzz_fn_defs(),
            "fuzz_fn_array_defs": self.fuzz_fn_array_defs()
        }

        return src_content % params

    def gen_fuzz_src_ser_tree(self):
        src_content = '''
#include <stdlib.h>
#include <string.h>

#include "tree.h"
#include "f1_c_fuzz.h"

extern node_t *_node_deserialize(const uint8_t *data_buf,
                                 size_t data_size, size_t *consumed_size);

int max_depth = -1;

static inline int map_rand(int v) {
  return random() %% v;
}
%(ser_tree_pool_defs)s

%(fuzz_fn_defs)s

%(fuzz_fn_array_defs)s

tree_t *gen_init__() {
  tree_t *tree = tree_create();
  tree->root = gen_funcs[1](0);
  return tree;
}'''

        params = {
            "string_pool_defs": self.string_pool_defs(),
            "ser_tree_pool_defs": self.ser_tree_pool_defs(),
            "fuzz_fn_defs": self.fuzz_fn_defs(),
            "fuzz_fn_array_defs": self.fuzz_fn_array_defs()
        }

        return src_content % params

    def fuzz_src(self):
        # return self.gen_fuzz_hdr(), self.gen_fuzz_src()
        # NOTE: use trees instead
        return self.gen_fuzz_hdr(), self.gen_fuzz_src_ser_tree()


def main(grammar, root_dir):
    random.seed(0)  # Fixed seed

    c_grammar = grammar

    hdr_path = os.path.join(root_dir, 'include/f1_c_fuzz.h')
    src_path = os.path.join(root_dir, 'src/f1_c_fuzz.c')
    fuzz_hdr, fuzz_src = CFuzzer(c_grammar).fuzz_src()
    with open(hdr_path, 'w') as f:
        print(fuzz_hdr, file=f)
    with open(src_path, 'w') as f:
        print(fuzz_src, file=f)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(sys.argv[0] + ' </path/to/grammar/file> </path/to/project/dir>')
        sys.exit(1)

    grammar_file_path = sys.argv[1]
    with open(grammar_file_path, 'r') as fp:
        main(json.load(fp), sys.argv[2])
