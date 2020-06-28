
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


void out(const char* str, const int str_l);

int map(int v);

extern int max_depth;
void gen_start(int depth);
void gen_json(int depth);
void gen_element(int depth);
void gen_value(int depth);
void gen_object(int depth);
void gen_members(int depth);
void gen_member(int depth);
void gen_array(int depth);
void gen_elements(int depth);
void gen_string(int depth);
void gen_characters(int depth);
void gen_character(int depth);
void gen_esc(int depth);
void gen_escc(int depth);
void gen_number(int depth);
void gen_int(int depth);
void gen_digits(int depth);
void gen_digit(int depth);
void gen_onenine(int depth);
void gen_frac(int depth);
void gen_exp(int depth);
void gen_sign(int depth);
void gen_ws(int depth);
void gen_sp1(int depth);
void gen_symbol(int depth);
void gen_symbol_1(int depth);
void gen_symbol_2(int depth);
void gen_symbol_1_1(int depth);
void gen_character_1(int depth);
void gen_digit_1(int depth);

const char* pool_start[] =  {"null", "true", "false"};
const int pool_l_start[] =  {4, 4, 5};


const char* pool_json[] =  {"null", "false", "true"};
const int pool_l_json[] =  {4, 5, 4};


const char* pool_element[] =  {"true", "null", "false"};
const int pool_l_element[] =  {4, 4, 5};


const char* pool_value[] =  {"false", "true", "null"};
const int pool_l_value[] =  {5, 4, 4};


const char* pool_object[] =  {"{}"};
const int pool_l_object[] =  {2};


const char* pool_members[] =  {"\"\":true", "\"\":null", "\"\":false"};
const int pool_l_members[] =  {7, 7, 8};


const char* pool_member[] =  {"\"\":false", "\"\":null", "\"\":true"};
const int pool_l_member[] =  {8, 7, 7};


const char* pool_array[] =  {"[]"};
const int pool_l_array[] =  {2};


const char* pool_elements[] =  {"null", "true", "false"};
const int pool_l_elements[] =  {4, 4, 5};


const char* pool_string[] =  {"\"\""};
const int pool_l_string[] =  {2};


const char* pool_characters[] =  {""};
const int pool_l_characters[] =  {0};


const char* pool_character[] =  {"^", "N", "S", "5", "l", "B", ",", "\"", "m", "Z", "6", "%", "q", "t", "h", "4", "T", "Q", "k", "8", "e", "x", ";", "s", "V", "_", "P", "G", "2", "D", "z", "-", "r", "`", "o", ".", " ", ":", "v", "y", "=", "g", "j", "w", "J", "U", "p", "1", "u", "K", "[", "!", "C", "]", "?", "M", "W", "}", "(", "@", "|", "#", "X", "*", "n", "O", "R", "9", "{", "b", ")", ">", "f", "A", "$", "&", "+", "E", "3", "F", "Y", "/", "H", "I", "a", "<", "c", "L", "d", "7", "0", "~", "i"};
const int pool_l_character[] =  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};


const char* pool_esc[] =  {"\\\"", "\\n", "\\\\", "\\b", "\\r", "\\t", "\\f"};
const int pool_l_esc[] =  {2, 2, 2, 2, 2, 2, 2};


const char* pool_escc[] =  {"b", "\"", "\\", "t", "r", "f", "n"};
const int pool_l_escc[] =  {1, 1, 1, 1, 1, 1, 1};


const char* pool_number[] =  {"0"};
const int pool_l_number[] =  {1};


const char* pool_int[] =  {"0"};
const int pool_l_int[] =  {1};


const char* pool_digits[] =  {"0"};
const int pool_l_digits[] =  {1};


const char* pool_digit[] =  {"0"};
const int pool_l_digit[] =  {1};


const char* pool_onenine[] =  {"9", "1", "2", "3", "6", "8", "5", "7", "4"};
const int pool_l_onenine[] =  {1, 1, 1, 1, 1, 1, 1, 1, 1};


const char* pool_frac[] =  {""};
const int pool_l_frac[] =  {0};


const char* pool_exp[] =  {""};
const int pool_l_exp[] =  {0};


const char* pool_sign[] =  {"-", "+", ""};
const int pool_l_sign[] =  {1, 1, 0};


const char* pool_ws[] =  {""};
const int pool_l_ws[] =  {0};


const char* pool_sp1[] =  {"\n", "\t", " ", "\r"};
const int pool_l_sp1[] =  {1, 1, 1, 1};


const char* pool_symbol[] =  {",\"\":true", ",\"\":null", ",\"\":false"};
const int pool_l_symbol[] =  {8, 8, 9};


const char* pool_symbol_1[] =  {",null", ",true", ",false"};
const int pool_l_symbol_1[] =  {5, 5, 6};


const char* pool_symbol_2[] =  {""};
const int pool_l_symbol_2[] =  {0};


const char* pool_symbol_1_1[] =  {""};
const int pool_l_symbol_1_1[] =  {0};


const char* pool_character_1[] =  {""};
const int pool_l_character_1[] =  {0};


const char* pool_digit_1[] =  {"0"};
const int pool_l_digit_1[] =  {1};


void gen_start(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_start[val];
        const int str_l = pool_l_start[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_json(depth +1);
        break;

    }
}


void gen_json(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_json[val];
        const int str_l = pool_l_json[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_element(depth +1);
        break;

    }
}


void gen_element(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_element[val];
        const int str_l = pool_l_element[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_ws(depth +1);
        gen_value(depth +1);
        gen_ws(depth +1);
        break;

    }
}


void gen_value(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_value[val];
        const int str_l = pool_l_value[val];
        out(str, str_l);
        return;
    }

    int val = map(7);
    switch(val) {

    case 0:
        out("false", 5);
        break;

    case 1:
        out("null", 4);
        break;

    case 2:
        out("true", 4);
        break;

    case 3:
        gen_array(depth +1);
        break;

    case 4:
        gen_object(depth +1);
        break;

    case 5:
        gen_number(depth +1);
        break;

    case 6:
        gen_string(depth +1);
        break;

    }
}


void gen_object(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_object[val];
        const int str_l = pool_l_object[val];
        out(str, str_l);
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:
        out("{", 1);
        gen_ws(depth +1);
        out("}", 1);
        break;

    case 1:
        out("{", 1);
        gen_members(depth +1);
        out("}", 1);
        break;

    }
}


void gen_members(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_members[val];
        const int str_l = pool_l_members[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_member(depth +1);
        gen_symbol_2(depth +1);
        break;

    }
}


void gen_member(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_member[val];
        const int str_l = pool_l_member[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_ws(depth +1);
        gen_string(depth +1);
        gen_ws(depth +1);
        out(":", 1);
        gen_element(depth +1);
        break;

    }
}


void gen_array(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_array[val];
        const int str_l = pool_l_array[val];
        out(str, str_l);
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:
        out("[", 1);
        gen_ws(depth +1);
        out("]", 1);
        break;

    case 1:
        out("[", 1);
        gen_elements(depth +1);
        out("]", 1);
        break;

    }
}


void gen_elements(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_elements[val];
        const int str_l = pool_l_elements[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_element(depth +1);
        gen_symbol_1_1(depth +1);
        break;

    }
}


void gen_string(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_string[val];
        const int str_l = pool_l_string[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        out("\"", 1);
        gen_characters(depth +1);
        out("\"", 1);
        break;

    }
}


void gen_characters(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_characters[val];
        const int str_l = pool_l_characters[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_character_1(depth +1);
        break;

    }
}


void gen_character(int depth) {
    if (depth > max_depth) {
        int val = map(93);
        const char* str = pool_character[val];
        const int str_l = pool_l_character[val];
        out(str, str_l);
        return;
    }

    int val = map(94);
    switch(val) {

    case 0:
        out(" ", 1);
        break;

    case 1:
        out("!", 1);
        break;

    case 2:
        out("\"", 1);
        break;

    case 3:
        out("#", 1);
        break;

    case 4:
        out("$", 1);
        break;

    case 5:
        out("%", 1);
        break;

    case 6:
        out("&", 1);
        break;

    case 7:
        out("(", 1);
        break;

    case 8:
        out(")", 1);
        break;

    case 9:
        out("*", 1);
        break;

    case 10:
        out("+", 1);
        break;

    case 11:
        out(",", 1);
        break;

    case 12:
        out("-", 1);
        break;

    case 13:
        out(".", 1);
        break;

    case 14:
        out("/", 1);
        break;

    case 15:
        out("0", 1);
        break;

    case 16:
        out("1", 1);
        break;

    case 17:
        out("2", 1);
        break;

    case 18:
        out("3", 1);
        break;

    case 19:
        out("4", 1);
        break;

    case 20:
        out("5", 1);
        break;

    case 21:
        out("6", 1);
        break;

    case 22:
        out("7", 1);
        break;

    case 23:
        out("8", 1);
        break;

    case 24:
        out("9", 1);
        break;

    case 25:
        out(":", 1);
        break;

    case 26:
        out(";", 1);
        break;

    case 27:
        out("<", 1);
        break;

    case 28:
        out("=", 1);
        break;

    case 29:
        out(">", 1);
        break;

    case 30:
        out("?", 1);
        break;

    case 31:
        out("@", 1);
        break;

    case 32:
        out("A", 1);
        break;

    case 33:
        out("B", 1);
        break;

    case 34:
        out("C", 1);
        break;

    case 35:
        out("D", 1);
        break;

    case 36:
        out("E", 1);
        break;

    case 37:
        out("F", 1);
        break;

    case 38:
        out("G", 1);
        break;

    case 39:
        out("H", 1);
        break;

    case 40:
        out("I", 1);
        break;

    case 41:
        out("J", 1);
        break;

    case 42:
        out("K", 1);
        break;

    case 43:
        out("L", 1);
        break;

    case 44:
        out("M", 1);
        break;

    case 45:
        out("N", 1);
        break;

    case 46:
        out("O", 1);
        break;

    case 47:
        out("P", 1);
        break;

    case 48:
        out("Q", 1);
        break;

    case 49:
        out("R", 1);
        break;

    case 50:
        out("S", 1);
        break;

    case 51:
        out("T", 1);
        break;

    case 52:
        out("U", 1);
        break;

    case 53:
        out("V", 1);
        break;

    case 54:
        out("W", 1);
        break;

    case 55:
        out("X", 1);
        break;

    case 56:
        out("Y", 1);
        break;

    case 57:
        out("Z", 1);
        break;

    case 58:
        out("[", 1);
        break;

    case 59:
        out("]", 1);
        break;

    case 60:
        out("^", 1);
        break;

    case 61:
        out("_", 1);
        break;

    case 62:
        out("`", 1);
        break;

    case 63:
        out("a", 1);
        break;

    case 64:
        out("b", 1);
        break;

    case 65:
        out("c", 1);
        break;

    case 66:
        out("d", 1);
        break;

    case 67:
        out("e", 1);
        break;

    case 68:
        out("f", 1);
        break;

    case 69:
        out("g", 1);
        break;

    case 70:
        out("h", 1);
        break;

    case 71:
        out("i", 1);
        break;

    case 72:
        out("j", 1);
        break;

    case 73:
        out("k", 1);
        break;

    case 74:
        out("l", 1);
        break;

    case 75:
        out("m", 1);
        break;

    case 76:
        out("n", 1);
        break;

    case 77:
        out("o", 1);
        break;

    case 78:
        out("p", 1);
        break;

    case 79:
        out("q", 1);
        break;

    case 80:
        out("r", 1);
        break;

    case 81:
        out("s", 1);
        break;

    case 82:
        out("t", 1);
        break;

    case 83:
        out("u", 1);
        break;

    case 84:
        out("v", 1);
        break;

    case 85:
        out("w", 1);
        break;

    case 86:
        out("x", 1);
        break;

    case 87:
        out("y", 1);
        break;

    case 88:
        out("z", 1);
        break;

    case 89:
        out("{", 1);
        break;

    case 90:
        out("|", 1);
        break;

    case 91:
        out("}", 1);
        break;

    case 92:
        out("~", 1);
        break;

    case 93:
        gen_esc(depth +1);
        break;

    }
}


void gen_esc(int depth) {
    if (depth > max_depth) {
        int val = map(7);
        const char* str = pool_esc[val];
        const int str_l = pool_l_esc[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        out("\\", 1);
        gen_escc(depth +1);
        break;

    }
}


void gen_escc(int depth) {
    if (depth > max_depth) {
        int val = map(7);
        const char* str = pool_escc[val];
        const int str_l = pool_l_escc[val];
        out(str, str_l);
        return;
    }

    int val = map(7);
    switch(val) {

    case 0:
        out("\"", 1);
        break;

    case 1:
        out("\\", 1);
        break;

    case 2:
        out("b", 1);
        break;

    case 3:
        out("f", 1);
        break;

    case 4:
        out("n", 1);
        break;

    case 5:
        out("r", 1);
        break;

    case 6:
        out("t", 1);
        break;

    }
}


void gen_number(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_number[val];
        const int str_l = pool_l_number[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_int(depth +1);
        gen_frac(depth +1);
        gen_exp(depth +1);
        break;

    }
}


void gen_int(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_int[val];
        const int str_l = pool_l_int[val];
        out(str, str_l);
        return;
    }

    int val = map(4);
    switch(val) {

    case 0:
        gen_digit(depth +1);
        break;

    case 1:
        out("-", 1);
        gen_digits(depth +1);
        break;

    case 2:
        out("-", 1);
        gen_onenine(depth +1);
        gen_digits(depth +1);
        break;

    case 3:
        gen_onenine(depth +1);
        gen_digits(depth +1);
        break;

    }
}


void gen_digits(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_digits[val];
        const int str_l = pool_l_digits[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_digit_1(depth +1);
        break;

    }
}


void gen_digit(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_digit[val];
        const int str_l = pool_l_digit[val];
        out(str, str_l);
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:
        out("0", 1);
        break;

    case 1:
        gen_onenine(depth +1);
        break;

    }
}


void gen_onenine(int depth) {
    if (depth > max_depth) {
        int val = map(9);
        const char* str = pool_onenine[val];
        const int str_l = pool_l_onenine[val];
        out(str, str_l);
        return;
    }

    int val = map(9);
    switch(val) {

    case 0:
        out("1", 1);
        break;

    case 1:
        out("2", 1);
        break;

    case 2:
        out("3", 1);
        break;

    case 3:
        out("4", 1);
        break;

    case 4:
        out("5", 1);
        break;

    case 5:
        out("6", 1);
        break;

    case 6:
        out("7", 1);
        break;

    case 7:
        out("8", 1);
        break;

    case 8:
        out("9", 1);
        break;

    }
}


void gen_frac(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_frac[val];
        const int str_l = pool_l_frac[val];
        out(str, str_l);
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:

        break;

    case 1:
        out(".", 1);
        gen_digits(depth +1);
        break;

    }
}


void gen_exp(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_exp[val];
        const int str_l = pool_l_exp[val];
        out(str, str_l);
        return;
    }

    int val = map(3);
    switch(val) {

    case 0:

        break;

    case 1:
        out("E", 1);
        gen_sign(depth +1);
        gen_digits(depth +1);
        break;

    case 2:
        out("e", 1);
        gen_sign(depth +1);
        gen_digits(depth +1);
        break;

    }
}


void gen_sign(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_sign[val];
        const int str_l = pool_l_sign[val];
        out(str, str_l);
        return;
    }

    int val = map(3);
    switch(val) {

    case 0:

        break;

    case 1:
        out("+", 1);
        break;

    case 2:
        out("-", 1);
        break;

    }
}


void gen_ws(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_ws[val];
        const int str_l = pool_l_ws[val];
        out(str, str_l);
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:

        break;

    case 1:
        gen_sp1(depth +1);
        gen_ws(depth +1);
        break;

    }
}


void gen_sp1(int depth) {
    if (depth > max_depth) {
        int val = map(4);
        const char* str = pool_sp1[val];
        const int str_l = pool_l_sp1[val];
        out(str, str_l);
        return;
    }

    int val = map(4);
    switch(val) {

    case 0:
        out("\t", 1);
        break;

    case 1:
        out("\n", 1);
        break;

    case 2:
        out("\r", 1);
        break;

    case 3:
        out(" ", 1);
        break;

    }
}


void gen_symbol(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_symbol[val];
        const int str_l = pool_l_symbol[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        out(",", 1);
        gen_members(depth +1);
        break;

    }
}


void gen_symbol_1(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_symbol_1[val];
        const int str_l = pool_l_symbol_1[val];
        out(str, str_l);
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        out(",", 1);
        gen_elements(depth +1);
        break;

    }
}


void gen_symbol_2(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_symbol_2[val];
        const int str_l = pool_l_symbol_2[val];
        out(str, str_l);
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:

        break;

    case 1:
        gen_symbol(depth +1);
        gen_symbol_2(depth +1);
        break;

    }
}


void gen_symbol_1_1(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_symbol_1_1[val];
        const int str_l = pool_l_symbol_1_1[val];
        out(str, str_l);
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:

        break;

    case 1:
        gen_symbol_1(depth +1);
        gen_symbol_1_1(depth +1);
        break;

    }
}


void gen_character_1(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_character_1[val];
        const int str_l = pool_l_character_1[val];
        out(str, str_l);
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:

        break;

    case 1:
        gen_character(depth +1);
        gen_character_1(depth +1);
        break;

    }
}


void gen_digit_1(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_digit_1[val];
        const int str_l = pool_l_digit_1[val];
        out(str, str_l);
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:
        gen_digit(depth +1);
        break;

    case 1:
        gen_digit(depth +1);
        gen_digit_1(depth +1);
        break;

    }
}


void gen_init__() {
    gen_start(0);
    out("\n", 1);
    return;
}
