grammar T;
s : ID* EOF ;
ID : [a-z]+ ;
WS : [ \t\r\n]+ -> skip ;
