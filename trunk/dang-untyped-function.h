
/* $untyped_function($params(...), body) */

typedef struct _DangUntypedFunctionImplicitParam DangUntypedFunctionImplicitParam;
struct _DangUntypedFunctionImplicitParam
{
  DangVarId var_id;
  char *name;
  DangValueType *type;
};

typedef struct _DangUntypedFunctionFailure DangUntypedFunctionFailure;
struct _DangUntypedFunctionFailure
{
  DangError *error;
  DangUntypedFunctionFailure *next;

  /* params follow */
};

typedef struct _DangUntypedFunctionReject DangUntypedFunctionReject;
struct _DangUntypedFunctionReject
{
  DangFunction *func;
  DangUntypedFunctionReject *next;
};

struct _DangUntypedFunction
{
  DangImports *imports;
  unsigned n_params;
  char **param_names;
  DangExpr *body;

  unsigned n_closure_params;
  DangUntypedFunctionImplicitParam *closure_params;

  /* Filled in by FunctionFamily's make_stub function. */
  DangFunction *func;

  /* Functions that compiled, but were not the right return-type */
  DangUntypedFunctionReject *rejects;

  /* Functions that didn't compile. */
  DangUntypedFunctionFailure *failures;
};

/* For use from try_sig implementations */
dang_boolean   dang_untyped_function_make_stub (DangUntypedFunction *untyped_function,
                                                DangFunctionParam   *params,
                                                DangError          **error);
dang_boolean dang_untyped_function_make_stub_from_sig (DangUntypedFunction *untyped,
                                                       DangSignature       *sig,
                                                       DangError          **error);

void dang_untyped_function_free (DangUntypedFunction *uf);


/* shared between untyped-functions and closures.  a hack. */
DangExpr *dang_untyped_function_wrap_in_return (DangExpr *expr);
