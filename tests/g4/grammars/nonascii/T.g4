grammar T;
program : statement* EOF ;
statement : ID '=' STRING ';' ;
ID : [a-zA-Z_]+ ;
STRING : '"' ~["]* '"' ;
COMMENT : '/*' .*? '*/' -> channel(HIDDEN) ;
WS : [ \t\r\n]+ -> skip ;
