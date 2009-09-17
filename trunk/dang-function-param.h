
typedef enum
{
  DANG_FUNCTION_PARAM_IN = 1,
  DANG_FUNCTION_PARAM_OUT = 2,
  DANG_FUNCTION_PARAM_INOUT = 3,
} DangFunctionParamDir;
const char *dang_function_param_dir_name (DangFunctionParamDir);

typedef struct _DangFunctionParam DangFunctionParam;
struct _DangFunctionParam
{
  DangFunctionParamDir dir;
  const char *name;
  DangValueType *type;
};

dang_boolean dang_function_param_parse (DangExpr *expr,
                                        DangFunctionParam *out,
                                        DangError **error);
