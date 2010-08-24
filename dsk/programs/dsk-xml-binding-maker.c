
/* type -> STRUCT opt_name LBRACE member_list RBRACE
         | BAREWORD
	 | UNION opt_name opt_extends LBRACE case_list RBRACE
	 ;
   opt_name -> BAREWORD
         |
	 ;
   member_list -> type BAREWORD SEMICOLON member_list
         |
	 ;
   case_list -> CASE BAREWORD(label) BAREWORD(type) SEMICOLON case_list
         | CASE BAREWORD(label) LBRACE member_list RBRACE SEMICOLON case_list
	 |
	 ;

 Builtin types:
    int
    uint
    int64
    uint64
    double
    string
 */
 
