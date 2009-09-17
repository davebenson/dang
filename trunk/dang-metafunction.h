
/* Phase 1: type annotation handler */
typedef dang_boolean (*DangMetafunctionAnnotateFunc)
                                           (DangAnnotations       *annotations,
                                            DangExpr              *expr,
                                            DangImports           *imports,
				            DangVarTable          *var_table,
				            DangError            **error);

#define DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(funcname)              \
  dang_boolean funcname (DangAnnotations *annotations,                 \
                         DangExpr    *expr,                            \
                         DangImports *imports,                         \
                         DangVarTable *var_table,                      \
                         DangError  **error)


/* Phase 2. compiler function */
typedef void (*DangMetafunctionCompileFunc)(DangExpr              *expr,
                                            DangBuilder   *builder,
                                            DangCompileFlags      *flags,
                                            DangCompileResult     *result);

#define DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(funcname)               \
    void funcname (DangExpr              *expr,                        \
                   DangBuilder   *builder,                     \
                   DangCompileFlags      *flags,                       \
                   DangCompileResult     *result)

typedef struct _DangMetafunction DangMetafunction;
struct _DangMetafunction
{
  const char *name;
  const char *syntax_check;       /* see doc/metafunction-syntax-check.txt */
  DangMetafunctionAnnotateFunc annotate;
  DangMetafunctionCompileFunc compile;
};

DangMetafunction *dang_metafunction_lookup (const char *name);
DangMetafunction *dang_metafunction_lookup_by_expr (DangExpr *expr);

extern DangMetafunction dang_bareword_metafunction;
extern DangMetafunction dang_value_metafunction;

dang_boolean dang_syntax_check (DangExpr *expr, DangError **error);
