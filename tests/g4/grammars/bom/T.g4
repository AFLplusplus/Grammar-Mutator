grammar T;
s : ID* EOF ;
ID : [a-z]+ ;
BOM : '﻿' -> channel(HIDDEN) ;
WS : ' ' -> skip ;
