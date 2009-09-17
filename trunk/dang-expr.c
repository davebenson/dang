#define _GNU_SOURCE
#include <string.h>
#include "dang.h"

const char *dang_expr_type_name (DangExprType type)
{
  switch (type)
    {
    case DANG_EXPR_TYPE_VALUE: return "value";
    case DANG_EXPR_TYPE_BAREWORD: return "bareword";
    case DANG_EXPR_TYPE_FUNCTION: return "function";
    default: return "*bad DangExprType*";
    }
}

static inline void
expr_base_init (DangExprAny *expr,
                DangExprType type)
{
  expr->type = type;
  dang_code_position_init (&expr->code_position);
  expr->ref_count = 1;
  expr->had_parentheses = 0;
}

DangExpr *dang_expr_new_bareword (const char       *str)
{
  unsigned len = strlen (str);
  size_t size = sizeof (DangExpr) + len + 1;
  DangExpr *expr;
  expr = dang_malloc (size);
  expr_base_init (&expr->any, DANG_EXPR_TYPE_BAREWORD);
  expr->bareword.name = (char*)(expr+1);
  memcpy (expr->bareword.name, str, len + 1);
  return expr;
}


DangExpr *dang_expr_new_function_take (const char *str,
                                  unsigned    n_args,
                                  DangExpr  **args)
{
  unsigned str_len = strlen (str);
  size_t size = sizeof (DangExpr)
              + sizeof (DangExpr *) * n_args
              + str_len + 1;
  unsigned i;
  DangExpr *expr;
  char *at;
  expr = dang_malloc (size);
  expr_base_init (&expr->any, DANG_EXPR_TYPE_FUNCTION);
  expr->function.n_args = n_args;
  expr->function.args = (DangExpr **)(expr + 1);
  at = (char*)(expr->function.args + n_args);
  expr->function.name = at;
  memcpy (at, str, str_len + 1);
  for (i = 0; i < n_args; i++)
    expr->function.args[i] = args[i];
  return expr;
}

DangExpr *dang_expr_new_function (const char *str,
                                  unsigned    n_args,
                                  DangExpr  **args)
{
  unsigned i;
  for (i = 0; i < n_args; i++)
    dang_expr_ref (args[i]);
  return dang_expr_new_function_take (str, n_args, args);
}

DangExpr *dang_expr_new_value    (DangValueType *type,
                                  const void    *value)
{
  DangExpr *expr = dang_malloc (sizeof (DangExpr) + type->sizeof_instance);
  expr_base_init (&expr->any, DANG_EXPR_TYPE_VALUE);
  expr->value.type = type;
  expr->value.value = expr + 1;
  if (type->init_assign)
    type->init_assign (type, expr->value.value, value);
  else
    memcpy (expr->value.value, value, type->sizeof_instance);
  return expr;
}

void      dang_expr_set_pos      (DangExpr   *expr,
                                  DangCodePosition *code_position)
{
  if (expr->any.code_position.filename == NULL)
    dang_code_position_copy (&expr->any.code_position, code_position);
}
DangExpr *dang_expr_ref          (DangExpr   *expr)
{
  ++(expr->any.ref_count);
  return expr;
}
void      dang_expr_unref        (DangExpr   *expr)
{
  unsigned i;
restart:
  if (--(expr->any.ref_count) != 0)
    return;
  switch (expr->type)
    {
    case DANG_EXPR_TYPE_BAREWORD:
      break;
    case DANG_EXPR_TYPE_FUNCTION:
      for (i = 0; i + 1 < expr->function.n_args; i++)
        dang_expr_unref (expr->function.args[i]);
      if (i + 1 == expr->function.n_args)
        {
          DangExpr *tail = expr->function.args[i];
          dang_code_position_clear (&expr->any.code_position);
          dang_free (expr);
          expr = tail;
          goto restart;
        }
      break;
    case DANG_EXPR_TYPE_VALUE:
      if (expr->value.type->destruct)
        (*expr->value.type->destruct) (expr->value.type, expr->value.value);
      break;
    }
  dang_code_position_clear (&expr->any.code_position);
  dang_free (expr);
}

/* helper functions */
dang_boolean dang_expr_is_function (DangExpr *expr,
                                    const char *func_name)
{
  return expr->type == DANG_EXPR_TYPE_FUNCTION
      && strcmp (expr->function.name, func_name) == 0;
}
