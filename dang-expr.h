
typedef struct _DangExprAny DangExprAny;
typedef struct _DangExprValue DangExprValue;
typedef struct _DangExprBareword DangExprBareword;
typedef struct _DangExprFunction DangExprFunction;

typedef struct _DangExprAnnotation DangExprAnnotation;


typedef enum
{
  DANG_EXPR_TYPE_VALUE,
  DANG_EXPR_TYPE_BAREWORD,
  DANG_EXPR_TYPE_FUNCTION
} DangExprType;
const char *dang_expr_type_name (DangExprType);

struct _DangExprAny
{
  DangExprType type;
  DangCodePosition code_position;
  unsigned ref_count;
  unsigned had_parentheses : 1;
};

struct _DangExprValue
{
  DangExprAny base;
  DangValueType *type;
  void *value;
};

struct _DangExprBareword
{
  DangExprAny base;
  char *name;
};

struct _DangExprFunction
{
  DangExprAny base;
  char       *name;
  unsigned    n_args;
  DangExpr  **args;
};

union _DangExpr
{
  DangExprType type;
  DangExprAny any;

  /* only the entry corresponding to 'type' is valid */
  DangExprValue value;
  DangExprBareword bareword;
  DangExprFunction function;
};

DangExpr *dang_expr_new_bareword (const char  *str);
DangExpr *dang_expr_new_function (const char  *str,
                                  unsigned    n_args,
                                  DangExpr  **args);
DangExpr *dang_expr_new_function_take (const char  *str,
                                  unsigned    n_args,
                                  DangExpr  **args);
DangExpr *dang_expr_new_value    (DangValueType *type,
                                  const void    *value);
void      dang_expr_set_pos      (DangExpr   *expr,
                                  DangCodePosition *code_position);
DangExpr *dang_expr_ref          (DangExpr   *expr);
void      dang_expr_unref        (DangExpr   *expr);

dang_boolean dang_expr_is_function (DangExpr *expr,
                                    const char *func_name);

