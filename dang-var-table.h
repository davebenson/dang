typedef struct _DangVarTableScope DangVarTableScope;
struct _DangVarTableScope
{
  DangUtilArray var_ids;
};

typedef struct _DangVarTableVariable DangVarTableVariable;
struct _DangVarTableVariable
{
  char *name;
  DangExpr *decl;
  DangExpr *type_expr;          /* expr where we figured the type from */
  DangValueType *type;
  dang_boolean is_param;
  DangFunctionParamDir param_dir;
};

struct _DangVarTable
{
  DangUtilArray variables;
  DangUtilArray scopes;
  dang_boolean has_rv;
};

/* a table structure for allocating local variables */
DangVarTable     *dang_var_table_new         (dang_boolean  has_rv);
void              dang_var_table_push        (DangVarTable *);
void              dang_var_table_pop         (DangVarTable *);
DangVarId         dang_var_table_alloc_local (DangVarTable *,
                                              const char *name,
                                              DangExpr *decl_expr,
                                              DangValueType *type);
dang_boolean      dang_var_table_lookup      (DangVarTable *,
                                              const char *name,
                                              DangVarId *var_id_out,
                                              DangValueType **type_out);
DangValueType *   dang_var_table_get_type    (DangVarTable *,
                                              DangVarId var_id);
const char       *dang_var_table_get_name    (DangVarTable *,
                                              DangVarId var_id);
void              dang_var_table_set_type    (DangVarTable *,
                                              DangVarId var_id,
                                              DangExpr *expr,
                                              DangValueType *type);
dang_boolean      dang_var_table_has_rv      (DangVarTable *);
#define dang_var_table_get_return_var_id(table)   0
#define dang_var_table_get_return_type(table) \
  dang_var_table_get_type((table), dang_var_table_get_return_var_id(table))

void dang_var_table_add_params (DangVarTable *,
                                DangValueType *rv_type, /* or NULL */
                                unsigned n_params,
                                DangFunctionParam *params);
void dang_var_table_free (DangVarTable *);
