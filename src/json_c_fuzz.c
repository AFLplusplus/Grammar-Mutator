
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "parsing_tree.h"


int map(int v);

extern int max_depth;
node_t *gen_start(int depth);
node_t *gen_json(int depth);
node_t *gen_element(int depth);
node_t *gen_value(int depth);
node_t *gen_object(int depth);
node_t *gen_members(int depth);
node_t *gen_member(int depth);
node_t *gen_array(int depth);
node_t *gen_elements(int depth);
node_t *gen_string(int depth);
node_t *gen_characters(int depth);
node_t *gen_character(int depth);
node_t *gen_esc(int depth);
node_t *gen_escc(int depth);
node_t *gen_number(int depth);
node_t *gen_int(int depth);
node_t *gen_digits(int depth);
node_t *gen_digit(int depth);
node_t *gen_onenine(int depth);
node_t *gen_frac(int depth);
node_t *gen_exp(int depth);
node_t *gen_sign(int depth);
node_t *gen_ws(int depth);
node_t *gen_sp1(int depth);
node_t *gen_symbol(int depth);
node_t *gen_symbol_1(int depth);
node_t *gen_symbol_2(int depth);
node_t *gen_symbol_1_1(int depth);
node_t *gen_character_1(int depth);
node_t *gen_digit_1(int depth);

const char* pool_start[] =  {"true", "null", "false"};
const int pool_l_start[] =  {4, 4, 5};


const char* pool_json[] =  {"null", "true", "false"};
const int pool_l_json[] =  {4, 4, 5};


const char* pool_element[] =  {"true", "null", "false"};
const int pool_l_element[] =  {4, 4, 5};


const char* pool_value[] =  {"false", "true", "null"};
const int pool_l_value[] =  {5, 4, 4};


const char* pool_object[] =  {"{}"};
const int pool_l_object[] =  {2};


const char* pool_members[] =  {"\"\":null", "\"\":true", "\"\":false"};
const int pool_l_members[] =  {7, 7, 8};


const char* pool_member[] =  {"\"\":null", "\"\":false", "\"\":true"};
const int pool_l_member[] =  {7, 8, 7};


const char* pool_array[] =  {"[]"};
const int pool_l_array[] =  {2};


const char* pool_elements[] =  {"true", "null", "false"};
const int pool_l_elements[] =  {4, 4, 5};


const char* pool_string[] =  {"\"\""};
const int pool_l_string[] =  {2};


const char* pool_characters[] =  {""};
const int pool_l_characters[] =  {0};


const char* pool_character[] =  {"s", "4", "w", "u", "A", "Y", ":", "c", "f", "B", "m", "&", ";", "J", "\"", "0", "9", "@", "<", "p", "U", "S", "i", ")", "5", "+", "n", "Q", "8", "k", "R", "3", "T", "7", "/", "}", "~", "I", "*", "F", "V", "-", "K", "G", "v", "C", "a", "2", "#", "P", "`", "O", "b", "M", "x", "y", "Z", "d", "(", "D", ",", "X", ">", "!", "t", "z", "E", "g", "N", "|", "W", "=", "o", " ", "e", "$", "^", "H", "_", "6", "h", "q", "L", ".", "r", "?", "l", "j", "]", "{", "%", "1", "["};
const int pool_l_character[] =  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};


const char* pool_esc[] =  {"\\\"", "\\t", "\\r", "\\\\", "\\b", "\\n", "\\f"};
const int pool_l_esc[] =  {2, 2, 2, 2, 2, 2, 2};


const char* pool_escc[] =  {"n", "\\", "r", "t", "f", "\"", "b"};
const int pool_l_escc[] =  {1, 1, 1, 1, 1, 1, 1};


const char* pool_number[] =  {"0"};
const int pool_l_number[] =  {1};


const char* pool_int[] =  {"0"};
const int pool_l_int[] =  {1};


const char* pool_digits[] =  {"0"};
const int pool_l_digits[] =  {1};


const char* pool_digit[] =  {"0"};
const int pool_l_digit[] =  {1};


const char* pool_onenine[] =  {"3", "4", "9", "8", "1", "5", "7", "2", "6"};
const int pool_l_onenine[] =  {1, 1, 1, 1, 1, 1, 1, 1, 1};


const char* pool_frac[] =  {""};
const int pool_l_frac[] =  {0};


const char* pool_exp[] =  {""};
const int pool_l_exp[] =  {0};


const char* pool_sign[] =  {"-", "+", ""};
const int pool_l_sign[] =  {1, 1, 0};


const char* pool_ws[] =  {""};
const int pool_l_ws[] =  {0};


const char* pool_sp1[] =  {"\t", " ", "\n", "\r"};
const int pool_l_sp1[] =  {1, 1, 1, 1};


const char* pool_symbol[] =  {",\"\":false", ",\"\":null", ",\"\":true"};
const int pool_l_symbol[] =  {9, 8, 8};


const char* pool_symbol_1[] =  {",false", ",null", ",true"};
const int pool_l_symbol_1[] =  {6, 5, 5};


const char* pool_symbol_2[] =  {""};
const int pool_l_symbol_2[] =  {0};


const char* pool_symbol_1_1[] =  {""};
const int pool_l_symbol_1_1[] =  {0};


const char* pool_character_1[] =  {""};
const int pool_l_character_1[] =  {0};


const char* pool_digit_1[] =  {"0"};
const int pool_l_digit_1[] =  {1};


node_t *gen_start(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_start[val];
        const int str_l = pool_l_start[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_json(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_json(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_json[val];
        const int str_l = pool_l_json[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_element(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_element(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_element[val];
        const int str_l = pool_l_element[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_ws(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_value(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_ws(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_value(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_value[val];
        const int str_l = pool_l_value[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(7);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val("false", 5);
        node_append_subnode(node, subnode);
        break;

    case 1:
        subnode = node_create_with_val("null", 4);
        node_append_subnode(node, subnode);
        break;

    case 2:
        subnode = node_create_with_val("true", 4);
        node_append_subnode(node, subnode);
        break;

    case 3:
        subnode = gen_array(depth +1);
        node_append_subnode(node, subnode);
        break;

    case 4:
        subnode = gen_object(depth +1);
        node_append_subnode(node, subnode);
        break;

    case 5:
        subnode = gen_number(depth +1);
        node_append_subnode(node, subnode);
        break;

    case 6:
        subnode = gen_string(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_object(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_object[val];
        const int str_l = pool_l_object[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(2);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val("{", 1);
        node_append_subnode(node, subnode);
        subnode = gen_ws(depth +1);
        node_append_subnode(node, subnode);
        subnode = node_create_with_val("}", 1);
        node_append_subnode(node, subnode);
        break;

    case 1:
        subnode = node_create_with_val("{", 1);
        node_append_subnode(node, subnode);
        subnode = gen_members(depth +1);
        node_append_subnode(node, subnode);
        subnode = node_create_with_val("}", 1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_members(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_members[val];
        const int str_l = pool_l_members[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_member(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_symbol_2(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_member(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_member[val];
        const int str_l = pool_l_member[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_ws(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_string(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_ws(depth +1);
        node_append_subnode(node, subnode);
        subnode = node_create_with_val(":", 1);
        node_append_subnode(node, subnode);
        subnode = gen_element(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_array(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_array[val];
        const int str_l = pool_l_array[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(2);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val("[", 1);
        node_append_subnode(node, subnode);
        subnode = gen_ws(depth +1);
        node_append_subnode(node, subnode);
        subnode = node_create_with_val("]", 1);
        node_append_subnode(node, subnode);
        break;

    case 1:
        subnode = node_create_with_val("[", 1);
        node_append_subnode(node, subnode);
        subnode = gen_elements(depth +1);
        node_append_subnode(node, subnode);
        subnode = node_create_with_val("]", 1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_elements(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_elements[val];
        const int str_l = pool_l_elements[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_element(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_symbol_1_1(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_string(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_string[val];
        const int str_l = pool_l_string[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val("\"", 1);
        node_append_subnode(node, subnode);
        subnode = gen_characters(depth +1);
        node_append_subnode(node, subnode);
        subnode = node_create_with_val("\"", 1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_characters(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_characters[val];
        const int str_l = pool_l_characters[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_character_1(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_character(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(93);
        const char* str = pool_character[val];
        const int str_l = pool_l_character[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(94);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val(" ", 1);
        node_append_subnode(node, subnode);
        break;

    case 1:
        subnode = node_create_with_val("!", 1);
        node_append_subnode(node, subnode);
        break;

    case 2:
        subnode = node_create_with_val("\"", 1);
        node_append_subnode(node, subnode);
        break;

    case 3:
        subnode = node_create_with_val("#", 1);
        node_append_subnode(node, subnode);
        break;

    case 4:
        subnode = node_create_with_val("$", 1);
        node_append_subnode(node, subnode);
        break;

    case 5:
        subnode = node_create_with_val("%", 1);
        node_append_subnode(node, subnode);
        break;

    case 6:
        subnode = node_create_with_val("&", 1);
        node_append_subnode(node, subnode);
        break;

    case 7:
        subnode = node_create_with_val("(", 1);
        node_append_subnode(node, subnode);
        break;

    case 8:
        subnode = node_create_with_val(")", 1);
        node_append_subnode(node, subnode);
        break;

    case 9:
        subnode = node_create_with_val("*", 1);
        node_append_subnode(node, subnode);
        break;

    case 10:
        subnode = node_create_with_val("+", 1);
        node_append_subnode(node, subnode);
        break;

    case 11:
        subnode = node_create_with_val(",", 1);
        node_append_subnode(node, subnode);
        break;

    case 12:
        subnode = node_create_with_val("-", 1);
        node_append_subnode(node, subnode);
        break;

    case 13:
        subnode = node_create_with_val(".", 1);
        node_append_subnode(node, subnode);
        break;

    case 14:
        subnode = node_create_with_val("/", 1);
        node_append_subnode(node, subnode);
        break;

    case 15:
        subnode = node_create_with_val("0", 1);
        node_append_subnode(node, subnode);
        break;

    case 16:
        subnode = node_create_with_val("1", 1);
        node_append_subnode(node, subnode);
        break;

    case 17:
        subnode = node_create_with_val("2", 1);
        node_append_subnode(node, subnode);
        break;

    case 18:
        subnode = node_create_with_val("3", 1);
        node_append_subnode(node, subnode);
        break;

    case 19:
        subnode = node_create_with_val("4", 1);
        node_append_subnode(node, subnode);
        break;

    case 20:
        subnode = node_create_with_val("5", 1);
        node_append_subnode(node, subnode);
        break;

    case 21:
        subnode = node_create_with_val("6", 1);
        node_append_subnode(node, subnode);
        break;

    case 22:
        subnode = node_create_with_val("7", 1);
        node_append_subnode(node, subnode);
        break;

    case 23:
        subnode = node_create_with_val("8", 1);
        node_append_subnode(node, subnode);
        break;

    case 24:
        subnode = node_create_with_val("9", 1);
        node_append_subnode(node, subnode);
        break;

    case 25:
        subnode = node_create_with_val(":", 1);
        node_append_subnode(node, subnode);
        break;

    case 26:
        subnode = node_create_with_val(";", 1);
        node_append_subnode(node, subnode);
        break;

    case 27:
        subnode = node_create_with_val("<", 1);
        node_append_subnode(node, subnode);
        break;

    case 28:
        subnode = node_create_with_val("=", 1);
        node_append_subnode(node, subnode);
        break;

    case 29:
        subnode = node_create_with_val(">", 1);
        node_append_subnode(node, subnode);
        break;

    case 30:
        subnode = node_create_with_val("?", 1);
        node_append_subnode(node, subnode);
        break;

    case 31:
        subnode = node_create_with_val("@", 1);
        node_append_subnode(node, subnode);
        break;

    case 32:
        subnode = node_create_with_val("A", 1);
        node_append_subnode(node, subnode);
        break;

    case 33:
        subnode = node_create_with_val("B", 1);
        node_append_subnode(node, subnode);
        break;

    case 34:
        subnode = node_create_with_val("C", 1);
        node_append_subnode(node, subnode);
        break;

    case 35:
        subnode = node_create_with_val("D", 1);
        node_append_subnode(node, subnode);
        break;

    case 36:
        subnode = node_create_with_val("E", 1);
        node_append_subnode(node, subnode);
        break;

    case 37:
        subnode = node_create_with_val("F", 1);
        node_append_subnode(node, subnode);
        break;

    case 38:
        subnode = node_create_with_val("G", 1);
        node_append_subnode(node, subnode);
        break;

    case 39:
        subnode = node_create_with_val("H", 1);
        node_append_subnode(node, subnode);
        break;

    case 40:
        subnode = node_create_with_val("I", 1);
        node_append_subnode(node, subnode);
        break;

    case 41:
        subnode = node_create_with_val("J", 1);
        node_append_subnode(node, subnode);
        break;

    case 42:
        subnode = node_create_with_val("K", 1);
        node_append_subnode(node, subnode);
        break;

    case 43:
        subnode = node_create_with_val("L", 1);
        node_append_subnode(node, subnode);
        break;

    case 44:
        subnode = node_create_with_val("M", 1);
        node_append_subnode(node, subnode);
        break;

    case 45:
        subnode = node_create_with_val("N", 1);
        node_append_subnode(node, subnode);
        break;

    case 46:
        subnode = node_create_with_val("O", 1);
        node_append_subnode(node, subnode);
        break;

    case 47:
        subnode = node_create_with_val("P", 1);
        node_append_subnode(node, subnode);
        break;

    case 48:
        subnode = node_create_with_val("Q", 1);
        node_append_subnode(node, subnode);
        break;

    case 49:
        subnode = node_create_with_val("R", 1);
        node_append_subnode(node, subnode);
        break;

    case 50:
        subnode = node_create_with_val("S", 1);
        node_append_subnode(node, subnode);
        break;

    case 51:
        subnode = node_create_with_val("T", 1);
        node_append_subnode(node, subnode);
        break;

    case 52:
        subnode = node_create_with_val("U", 1);
        node_append_subnode(node, subnode);
        break;

    case 53:
        subnode = node_create_with_val("V", 1);
        node_append_subnode(node, subnode);
        break;

    case 54:
        subnode = node_create_with_val("W", 1);
        node_append_subnode(node, subnode);
        break;

    case 55:
        subnode = node_create_with_val("X", 1);
        node_append_subnode(node, subnode);
        break;

    case 56:
        subnode = node_create_with_val("Y", 1);
        node_append_subnode(node, subnode);
        break;

    case 57:
        subnode = node_create_with_val("Z", 1);
        node_append_subnode(node, subnode);
        break;

    case 58:
        subnode = node_create_with_val("[", 1);
        node_append_subnode(node, subnode);
        break;

    case 59:
        subnode = node_create_with_val("]", 1);
        node_append_subnode(node, subnode);
        break;

    case 60:
        subnode = node_create_with_val("^", 1);
        node_append_subnode(node, subnode);
        break;

    case 61:
        subnode = node_create_with_val("_", 1);
        node_append_subnode(node, subnode);
        break;

    case 62:
        subnode = node_create_with_val("`", 1);
        node_append_subnode(node, subnode);
        break;

    case 63:
        subnode = node_create_with_val("a", 1);
        node_append_subnode(node, subnode);
        break;

    case 64:
        subnode = node_create_with_val("b", 1);
        node_append_subnode(node, subnode);
        break;

    case 65:
        subnode = node_create_with_val("c", 1);
        node_append_subnode(node, subnode);
        break;

    case 66:
        subnode = node_create_with_val("d", 1);
        node_append_subnode(node, subnode);
        break;

    case 67:
        subnode = node_create_with_val("e", 1);
        node_append_subnode(node, subnode);
        break;

    case 68:
        subnode = node_create_with_val("f", 1);
        node_append_subnode(node, subnode);
        break;

    case 69:
        subnode = node_create_with_val("g", 1);
        node_append_subnode(node, subnode);
        break;

    case 70:
        subnode = node_create_with_val("h", 1);
        node_append_subnode(node, subnode);
        break;

    case 71:
        subnode = node_create_with_val("i", 1);
        node_append_subnode(node, subnode);
        break;

    case 72:
        subnode = node_create_with_val("j", 1);
        node_append_subnode(node, subnode);
        break;

    case 73:
        subnode = node_create_with_val("k", 1);
        node_append_subnode(node, subnode);
        break;

    case 74:
        subnode = node_create_with_val("l", 1);
        node_append_subnode(node, subnode);
        break;

    case 75:
        subnode = node_create_with_val("m", 1);
        node_append_subnode(node, subnode);
        break;

    case 76:
        subnode = node_create_with_val("n", 1);
        node_append_subnode(node, subnode);
        break;

    case 77:
        subnode = node_create_with_val("o", 1);
        node_append_subnode(node, subnode);
        break;

    case 78:
        subnode = node_create_with_val("p", 1);
        node_append_subnode(node, subnode);
        break;

    case 79:
        subnode = node_create_with_val("q", 1);
        node_append_subnode(node, subnode);
        break;

    case 80:
        subnode = node_create_with_val("r", 1);
        node_append_subnode(node, subnode);
        break;

    case 81:
        subnode = node_create_with_val("s", 1);
        node_append_subnode(node, subnode);
        break;

    case 82:
        subnode = node_create_with_val("t", 1);
        node_append_subnode(node, subnode);
        break;

    case 83:
        subnode = node_create_with_val("u", 1);
        node_append_subnode(node, subnode);
        break;

    case 84:
        subnode = node_create_with_val("v", 1);
        node_append_subnode(node, subnode);
        break;

    case 85:
        subnode = node_create_with_val("w", 1);
        node_append_subnode(node, subnode);
        break;

    case 86:
        subnode = node_create_with_val("x", 1);
        node_append_subnode(node, subnode);
        break;

    case 87:
        subnode = node_create_with_val("y", 1);
        node_append_subnode(node, subnode);
        break;

    case 88:
        subnode = node_create_with_val("z", 1);
        node_append_subnode(node, subnode);
        break;

    case 89:
        subnode = node_create_with_val("{", 1);
        node_append_subnode(node, subnode);
        break;

    case 90:
        subnode = node_create_with_val("|", 1);
        node_append_subnode(node, subnode);
        break;

    case 91:
        subnode = node_create_with_val("}", 1);
        node_append_subnode(node, subnode);
        break;

    case 92:
        subnode = node_create_with_val("~", 1);
        node_append_subnode(node, subnode);
        break;

    case 93:
        subnode = gen_esc(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_esc(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(7);
        const char* str = pool_esc[val];
        const int str_l = pool_l_esc[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val("\\", 1);
        node_append_subnode(node, subnode);
        subnode = gen_escc(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_escc(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(7);
        const char* str = pool_escc[val];
        const int str_l = pool_l_escc[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(7);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val("\"", 1);
        node_append_subnode(node, subnode);
        break;

    case 1:
        subnode = node_create_with_val("\\", 1);
        node_append_subnode(node, subnode);
        break;

    case 2:
        subnode = node_create_with_val("b", 1);
        node_append_subnode(node, subnode);
        break;

    case 3:
        subnode = node_create_with_val("f", 1);
        node_append_subnode(node, subnode);
        break;

    case 4:
        subnode = node_create_with_val("n", 1);
        node_append_subnode(node, subnode);
        break;

    case 5:
        subnode = node_create_with_val("r", 1);
        node_append_subnode(node, subnode);
        break;

    case 6:
        subnode = node_create_with_val("t", 1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_number(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_number[val];
        const int str_l = pool_l_number[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_int(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_frac(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_exp(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_int(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_int[val];
        const int str_l = pool_l_int[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(4);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_digit(depth +1);
        node_append_subnode(node, subnode);
        break;

    case 1:
        subnode = node_create_with_val("-", 1);
        node_append_subnode(node, subnode);
        subnode = gen_digits(depth +1);
        node_append_subnode(node, subnode);
        break;

    case 2:
        subnode = node_create_with_val("-", 1);
        node_append_subnode(node, subnode);
        subnode = gen_onenine(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_digits(depth +1);
        node_append_subnode(node, subnode);
        break;

    case 3:
        subnode = gen_onenine(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_digits(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_digits(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_digits[val];
        const int str_l = pool_l_digits[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_digit_1(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_digit(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_digit[val];
        const int str_l = pool_l_digit[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(2);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val("0", 1);
        node_append_subnode(node, subnode);
        break;

    case 1:
        subnode = gen_onenine(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_onenine(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(9);
        const char* str = pool_onenine[val];
        const int str_l = pool_l_onenine[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(9);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val("1", 1);
        node_append_subnode(node, subnode);
        break;

    case 1:
        subnode = node_create_with_val("2", 1);
        node_append_subnode(node, subnode);
        break;

    case 2:
        subnode = node_create_with_val("3", 1);
        node_append_subnode(node, subnode);
        break;

    case 3:
        subnode = node_create_with_val("4", 1);
        node_append_subnode(node, subnode);
        break;

    case 4:
        subnode = node_create_with_val("5", 1);
        node_append_subnode(node, subnode);
        break;

    case 5:
        subnode = node_create_with_val("6", 1);
        node_append_subnode(node, subnode);
        break;

    case 6:
        subnode = node_create_with_val("7", 1);
        node_append_subnode(node, subnode);
        break;

    case 7:
        subnode = node_create_with_val("8", 1);
        node_append_subnode(node, subnode);
        break;

    case 8:
        subnode = node_create_with_val("9", 1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_frac(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_frac[val];
        const int str_l = pool_l_frac[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(2);
    node_t *subnode = NULL;
    switch(val) {

    case 0:

        break;

    case 1:
        subnode = node_create_with_val(".", 1);
        node_append_subnode(node, subnode);
        subnode = gen_digits(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_exp(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_exp[val];
        const int str_l = pool_l_exp[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(3);
    node_t *subnode = NULL;
    switch(val) {

    case 0:

        break;

    case 1:
        subnode = node_create_with_val("E", 1);
        node_append_subnode(node, subnode);
        subnode = gen_sign(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_digits(depth +1);
        node_append_subnode(node, subnode);
        break;

    case 2:
        subnode = node_create_with_val("e", 1);
        node_append_subnode(node, subnode);
        subnode = gen_sign(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_digits(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_sign(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_sign[val];
        const int str_l = pool_l_sign[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(3);
    node_t *subnode = NULL;
    switch(val) {

    case 0:

        break;

    case 1:
        subnode = node_create_with_val("+", 1);
        node_append_subnode(node, subnode);
        break;

    case 2:
        subnode = node_create_with_val("-", 1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_ws(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_ws[val];
        const int str_l = pool_l_ws[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(2);
    node_t *subnode = NULL;
    switch(val) {

    case 0:

        break;

    case 1:
        subnode = gen_sp1(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_ws(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_sp1(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(4);
        const char* str = pool_sp1[val];
        const int str_l = pool_l_sp1[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(4);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val("\t", 1);
        node_append_subnode(node, subnode);
        break;

    case 1:
        subnode = node_create_with_val("\n", 1);
        node_append_subnode(node, subnode);
        break;

    case 2:
        subnode = node_create_with_val("\r", 1);
        node_append_subnode(node, subnode);
        break;

    case 3:
        subnode = node_create_with_val(" ", 1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_symbol(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_symbol[val];
        const int str_l = pool_l_symbol[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val(",", 1);
        node_append_subnode(node, subnode);
        subnode = gen_members(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_symbol_1(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_symbol_1[val];
        const int str_l = pool_l_symbol_1[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(1);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = node_create_with_val(",", 1);
        node_append_subnode(node, subnode);
        subnode = gen_elements(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_symbol_2(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_symbol_2[val];
        const int str_l = pool_l_symbol_2[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(2);
    node_t *subnode = NULL;
    switch(val) {

    case 0:

        break;

    case 1:
        subnode = gen_symbol(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_symbol_2(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_symbol_1_1(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_symbol_1_1[val];
        const int str_l = pool_l_symbol_1_1[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(2);
    node_t *subnode = NULL;
    switch(val) {

    case 0:

        break;

    case 1:
        subnode = gen_symbol_1(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_symbol_1_1(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_character_1(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_character_1[val];
        const int str_l = pool_l_character_1[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(2);
    node_t *subnode = NULL;
    switch(val) {

    case 0:

        break;

    case 1:
        subnode = gen_character(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_character_1(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


node_t *gen_digit_1(int depth) {
    node_t *node = node_create();

    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_digit_1[val];
        const int str_l = pool_l_digit_1[val];
        node_set_val(node, str, str_l);
        return node;
    }

    int val = map(2);
    node_t *subnode = NULL;
    switch(val) {

    case 0:
        subnode = gen_digit(depth +1);
        node_append_subnode(node, subnode);
        break;

    case 1:
        subnode = gen_digit(depth +1);
        node_append_subnode(node, subnode);
        subnode = gen_digit_1(depth +1);
        node_append_subnode(node, subnode);
        break;

    }

    return node;
}


parsing_tree_t *gen_init__() {
    parsing_tree_t *tree = tree_create();
    tree->root = gen_start(0);
    return tree;
}
