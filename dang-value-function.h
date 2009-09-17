

typedef struct _DangValueTypeFunction DangValueTypeFunction;

struct _DangValueTypeFunction
{
  DangValueType base_type;
  DangSignature *sig;		/* no names */

  DangValueTypeFunction *left, *right, *parent;
  dang_boolean is_red;
};

DangValueType *dang_value_type_function (DangSignature *sig);

dang_boolean dang_value_type_is_function (DangValueType *type);

/* value of this is a pointer (ref-counted) on a FunctionFamily. */
DangValueType *dang_value_type_function_family (void);

