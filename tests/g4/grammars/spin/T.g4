grammar T;
options { superClass = SpinBase; }
@parser::header {#include "SpinBase.h"}
s : 'rules' a* EOF | 'loop' ({spin()}? ID | ID)* EOF ;
a : {spin()}? ID ;
ID : [a-z]+ ;
WS : ' ' -> skip ;
