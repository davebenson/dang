

#define DANG_DEBUG 1

#ifdef DANG_DEBUG
extern dang_boolean dang_debug_parse;
extern dang_boolean dang_debug_disassemble;
void dang_debug_dump_expr (DangExpr *expr);

void dang_debug_register_simple_c (DangSimpleCFunc func,
                                   DangNamespace  *ns,
                                   const char     *name);
dang_boolean dang_debug_query_simple_c (DangSimpleCFunc func,
                                        DangNamespace **ns_out,
                                        const char **name_out);


/* free up anything we can */
void _dang_debug_cleanup ();

#endif
