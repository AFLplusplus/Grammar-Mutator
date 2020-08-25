#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Date    : 2020-06-19 14:45:19
# @Author  : Shengtuo Hu
# @Link    : https://shengtuo.me

import sys
import itertools
import random
import os
import string
import json


class TreeNode:
    node_type: int = 0
    rule_id: int = 0
    val: str = ''
    parent: 'TreeNode' = None
    subnodes: list = None

    def __init__(self, node_type=0, rule_id=0, val='', subnodes=None):
        super().__init__()

        self.node_type = node_type
        self.rule_id = rule_id
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
        # rule id
        ret += self.rule_id.to_bytes(4, byteorder='little', signed=False)
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
        # rule id
        node.rule_id = int.from_bytes(data[consumed:consumed + 4], byteorder='little', signed=False)
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
        self.grammar_keys = list(self.grammar.keys())


class LimitFuzzer(Fuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        self.key_cost = {}
        self.cost = self.compute_cost(grammar)

    def symbol_cost(self, grammar, symbol):
        if symbol not in grammar:
            return 0  # terminal node
        if symbol in self.key_cost:
            return self.key_cost[symbol]
        return float("inf")

    def expansion_cost(self, grammar, rule):
        ret = 1
        for token in rule:
            if token not in grammar:
                continue
            ret += self.symbol_cost(grammar, token)
            if ret == float("inf"):
                return ret
        return ret

    def compute_cost(self, grammar):
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


class PooledFuzzer(LimitFuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        self.c_grammar = self.cheap_grammar()
        self.c_grammar_keys = list(self.c_grammar.keys())

        self.MAX_SAMPLE = 255

        # reorder our grammar rules by cost.
        for k in self.grammar_keys:
            self.grammar[k] = [r for (i, r) in self.cost[k]]
        self.ordered_grammar = True

        self.pool_of_trees = self.completion_trees()

    def cheap_grammar(self):
        new_grammar = {}
        for k in self.cost:
            crules = self.cost[k]
            min_cost = crules[0][0]
            new_grammar[k] = [r for c, r in crules if c == min_cost]
            assert len(new_grammar[k]) > 0
        return new_grammar

    def k_to_id(self, k):
        return self.grammar_keys.index(k) + 1

    def get_trees_for_key(self, grammar, key='<start>'):
        if key not in grammar:
            return [TreeNode(node_type=0, val=key)]
        v = sum([self.get_trees_for_rule(grammar, key, rule_id, rule)
                 for rule_id, rule in enumerate(grammar[key])], [])
        return random.sample(v, min(self.MAX_SAMPLE, len(v)))

    def get_trees_for_rule(self, grammar, key, rule_id, rule):
        assert grammar[key][rule_id] == rule
        my_trees_list = [
            self.get_trees_for_key(grammar, key) for key in rule]
        v = [TreeNode(node_type=self.k_to_id(key), rule_id=rule_id,
                      subnodes=subnodes)
             for subnodes in itertools.product(*my_trees_list)]
        return random.sample(v, min(self.MAX_SAMPLE, len(v)))

    def completion_trees(self):
        return {k: self.get_trees_for_key(self.c_grammar, k)
                for k in self.c_grammar_keys}


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
        for k in self.grammar_keys:
            for i_rule, rule in enumerate(self.grammar[k]):
                n = self.kr_to_s(k, i_rule)
                self.rule_recursion[n] = self.is_rule_recursive(n, rule, set())
        for k in self.grammar_keys:
            self.key_recursion[k] = self.is_key_recursive(k, k, set())


class CFuzzer(PyRecCompiledFuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)

    def gen_rule_src(self, rule, key, min_rule_cost):
        res = []
        ntokens = len(rule)
        nkeys = len([token for token in rule if token in self.grammar])
        res.append('remaining_len = max_len - %d;' % min_rule_cost)
        res.append(
            'node->subnodes = (node_t**)malloc(%d * sizeof(node_t*));' % (
                ntokens))
        res.append('node->subnode_count = %d;' % ntokens)
        for i, token in enumerate(rule):
            if token in self.grammar:
                res.append('subnode_max_len = get_random_len(%d, remaining_len);' % nkeys)
                nkeys -= 1
                res.append('subnode_max_len += %d;' % self.key_cost[token])
                res.append('remaining_len += %d;' % self.key_cost[token])
                res.append('subnode = gen_node_%s(subnode_max_len, &subnode_consumed, -1);' % self.k_to_s(token))
                res.append('remaining_len -= subnode_consumed;')
                res.append('*consumed += subnode_consumed;')
                res.append('node->non_term_size += 1;')
                if key == token:
                    res.append('node->recursion_edge_size += 1;')
            else:
                esc_token_chars = [self.esc_char(c) for c in token]
                esc_token = ''.join(esc_token_chars)
                res.append(
                    'subnode = node_create_with_val(NODE_TERM__, "%s", %d);' % (
                        esc_token, len(esc_token_chars)))
            res.append('node->subnodes[%d] = subnode;' % i)
        return '\n    '.join(res)

    def gen_num_candidate_rules(self, k):
        min_rule_sizes = []
        num_min_rules = []
        for min_rule_size, _ in self.cost[k]:
            if min_rule_size not in min_rule_sizes:
                min_rule_sizes.append(min_rule_size)
                num_min_rules.append(0)
            num_min_rules[-1] += 1

        res = []
        num_candidate_rules = 0
        min_rule_sizes = min_rule_sizes[1:]
        for i, min_rule_size in enumerate(min_rule_sizes):
            num_candidate_rules += num_min_rules[i]
            if i == 0:
                res.append('if (max_len < %d) {' % min_rule_size)
            else:
                res.append('} else if (max_len < %d) {' % min_rule_size)
            res.append('  num_rules = %d;' % num_candidate_rules)

        num_candidate_rules += num_min_rules[-1]
        if len(num_min_rules) == 1:
            res.append('num_rules = %d;' % num_candidate_rules)
        else:
            res.append('} else {')
            res.append('  num_rules = %d;' % num_candidate_rules)
            res.append('}')
        return '\n    '.join(res)

    def gen_alt_src_ser_tree(self, k):
        rules = self.grammar[k]
        cheap_trees = self.pool_of_trees[k]
        min_cost = self.cost[k][0][0]
        num_min_rules = len(self.c_grammar[k])
        result = list()
        result.append('''
node_t *gen_node_%(name)s(int max_len, int *consumed, int rule_index) {
  node_t *node = NULL;
  int val;

  if (max_len < %(min_cost)d) {
    val = map_rand(%(num_cheap_trees)d);
    size_t consumed = 0;
    const char* ser_data = pool_ser_%(name)s[val];
    const int ser_data_l = pool_l_ser_%(name)s[val];
    node = _node_deserialize(ser_data, ser_data_l, &consumed);
    return node;
  }

  if (rule_index < 0 || rule_index >= %(nrules)d) {
    int num_rules = 0;
    %(gen_num_candidate_rules)s

    val = 0;
    if (num_rules == %(num_min_rules)d) {
      val = map_rand(%(num_min_rules)d);
    } else {
      val = map_rand(num_rules - %(num_min_rules)d) + %(num_min_rules)d;
    }
  } else {
    val = rule_index;
  }

  node = node_create_with_rule_id(NODE_%(node_type)s, rule_index);

  *consumed = 1;
  int remaining_len = 0;
  int subnode_max_len = 0;
  int subnode_consumed = 0;

  node_t *subnode = NULL;
  switch(val) {''' % {
            'node_type': self.k_to_s(k).upper(),
            'name': self.k_to_s(k),
            'nrules': len(rules),
            'num_cheap_trees': len(cheap_trees),
            'min_cost': min_cost,
            'num_min_rules': num_min_rules,
            'gen_num_candidate_rules': self.gen_num_candidate_rules(k)
        })

        for i, rule in enumerate(rules):
            result.append('''
  case %d:
    %s
    break;''' % (i, self.gen_rule_src(rule, k, self.cost[k][i][0])))

        result.append('''
  }

  return node;
}''')
        return '\n'.join(result)

    def ser_tree_pool_defs(self):
        result = []
        for k in self.grammar_keys:
            cheap_trees = self.pool_of_trees[k]
            ser_cheap_trees = [tree.to_bytes() for tree in cheap_trees]
            ser_cheap_trees_c_str = [bytes_to_c_str(ser_tree) for ser_tree in ser_cheap_trees]
            result.append('''
const char* pool_ser_%(k)s[] = {%(ser_trees)s};
const int pool_l_ser_%(k)s[] = {%(ser_trees_len)s};''' % {
                'k': self.k_to_s(k),
                'ser_trees': ', '.join(['"%s"' % ser_tree_c_str for ser_tree_c_str in ser_cheap_trees_c_str]),
                'ser_trees_len': ', '.join([str(len(ser_tree)) for ser_tree in ser_cheap_trees])})
        return '\n'.join(result)

    def fuzz_fn_decs(self):
        result = []
        for k in self.grammar_keys:
            result.append('''node_t *gen_node_%s(int max_len, int *consumed, int rule_index);''' % self.k_to_s(k))
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
  NODE_TERM__ = 0,
  %s
};'''
        node_types = []
        for k in self.grammar_keys:
            node_types.append('NODE_%s,' % self.k_to_s(k).upper())
        return result % ('\n  '.join(node_types))

    def fuzz_fn_array_defs(self):
        result = '''
gen_func_t gen_funcs[%d] = {
  NULL,
  %s
};'''
        fuzz_fn_names = []
        for k in self.grammar_keys:
            fuzz_fn_names.append('gen_node_%s,' % self.k_to_s(k))
        return result % (
            len(self.grammar_keys) + 1,
            '\n  '.join(fuzz_fn_names))

    def node_type_str_defs(self):
        result = '''
const char *node_type_str(int node_type) {
  switch (node_type) {
  case 0: return "NODE_TERM__";
  %s
  default: return "";
  }
}'''
        node_type_strs = []
        for k in self.grammar_keys:
            node_type_strs.append('case %d: return "NODE_%s";' % (
                self.k_to_id(k), self.k_to_s(k).upper()))
        return result % ('\n  '.join(node_type_strs))

    def fuzz_node_cost_array_defs(self):
        result = '''
int node_min_lens[%d] = {
  0,
  %s
};'''
        node_min_lens = []
        for k in self.grammar_keys:
            node_min_lens.append('%d,' % self.cost[k][0][0])
        return result % (
            len(self.grammar_keys) + 1,
            '\n  '.join(node_min_lens))

    def gen_fuzz_hdr(self):
        hdr_content = '''
#ifndef __F1_C_FUZZ_H__
#define __F1_C_FUZZ_H__

#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

%(fuzz_fn_decs)s

tree_t *gen_init__(int max_len);

%(node_type_decs)s
const char *node_type_str(int node_type);

typedef node_t *(*gen_func_t)(int max_len, int *consumed, int rule_index);
extern gen_func_t gen_funcs[%(num_nodes)d];
extern int node_min_lens[%(num_nodes)d];

#ifdef __cplusplus
}
#endif

#endif'''

        params = {
            "fuzz_fn_decs": self.fuzz_fn_decs(),
            "node_type_decs": self.node_type_decs(),
            "num_nodes": len(self.grammar_keys) + 1
        }

        return hdr_content % params

    def gen_fuzz_src(self):
        src_content = '''
#include <stdlib.h>
#include <string.h>

#include "tree.h"
#include "f1_c_fuzz.h"

extern node_t *_node_deserialize(const uint8_t *data_buf,
                                 size_t data_size, size_t *consumed_size);

static inline int map_rand(int v) {
  return random() %% v;
}

static int get_random_len(int num_subnodes, int total_remaining_len) {
  int ret = total_remaining_len;
  int temp = 0;
  for (int i = 0; i < num_subnodes - 1; ++i) {
    temp = map_rand(total_remaining_len + 1);
    if (temp < ret) ret = temp;
  }
  return ret;
}
%(node_type_str_defs)s
%(ser_tree_pool_defs)s
%(fuzz_fn_defs)s
%(fuzz_fn_array_defs)s
%(fuzz_node_cost_array_defs)s

tree_t *gen_init__(int max_len) {
  tree_t *tree = tree_create();
  int consumed = 0;
  tree->root = gen_funcs[1](max_len, &consumed, -1);
  return tree;
}'''

        params = {
            "ser_tree_pool_defs": self.ser_tree_pool_defs(),
            "fuzz_fn_defs": self.fuzz_fn_defs(),
            "fuzz_fn_array_defs": self.fuzz_fn_array_defs(),
            "node_type_str_defs": self.node_type_str_defs(),
            "fuzz_node_cost_array_defs": self.fuzz_node_cost_array_defs()
        }

        return src_content % params

    def fuzz_src(self):
        return self.gen_fuzz_hdr(), self.gen_fuzz_src()


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
        print(sys.argv[0] + ' </path/to/grammar/file> </path/to/output/dir>')
        sys.exit(1)

    grammar_file_path = sys.argv[1]
    with open(grammar_file_path, 'r') as fp:
        main(json.load(fp), sys.argv[2])
