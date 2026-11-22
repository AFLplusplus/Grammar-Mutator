grammar T;
options { superClass = Base; }
@parser::header {#include "Base.h"}
s : {check()}? ID ;
ID : [a-z]+ ;
