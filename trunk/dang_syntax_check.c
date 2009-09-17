#include "dang.h"
#include <string.h>

static inline dang_boolean
is_stdint_type (DangValueType *type)
{
  return type == dang_value_type_int32 ()
      || type == dang_value_type_uint32 ();
}
static void
skip_syntax_check (const char **pat)
{
  switch (**pat)
    {
    case 'A':
    case 'B':
    case 'V':
    case 'T':
    case 'I':
      *pat += 1;
      break;
    case '|':
      {
        dang_assert ((*pat)[1] == '(');
        *pat += 2;
        for (;;)
          {
            skip_syntax_check (pat);
            if (**pat == ')')
              {
                *pat += 1;
                return;
              }
            else
              {
                assert (**pat == '|');
                *pat += 1;
              }
          }
        break;
      }
    case '*':
      {
        *pat += 1;
        skip_syntax_check (pat);
        break;
      }
    case 0:
      dang_assert_not_reached ();
      break;
    default:
      {
        const char *p = strchr (*pat, ')');
        assert (p);
        *pat = p + 1;
        break;
      }
    }
}

static dang_boolean
syntax_check_mostly_nonrecursive (const char *pattern,
                        unsigned   *pattern_used_out,
                        DangExpr   *expr,
                        unsigned   *expr_index_inout,
                        DangError **error)
{
  switch (*pattern)
    {
    case 'A':
      if (*expr_index_inout == expr->function.n_args)
        {
          dang_set_error (error, "missing argument #%u to %s()",
                          *expr_index_inout + 1, expr->function.name);
          return FALSE;
        }
      *expr_index_inout += 1;
      *pattern_used_out = 1;
      return TRUE;
    case 'B':
      if (*expr_index_inout == expr->function.n_args)
        {
          dang_set_error (error, "missing bareword argument #%u to %s()",
                          *expr_index_inout + 1, expr->function.name);
          return FALSE;
        }
      if (expr->function.args[*expr_index_inout]->type != DANG_EXPR_TYPE_BAREWORD)
        {
          dang_set_error (error, "argument #%u to %s() not a bareword",
                          *expr_index_inout + 1, expr->function.name);
          return FALSE;
        }
      *expr_index_inout += 1;
      *pattern_used_out = 1;
      return TRUE;
    case '@':
      *pattern_used_out = 1;
      *expr_index_inout = expr->function.n_args;
      return TRUE;
    case 'V':
      if (*expr_index_inout == expr->function.n_args)
        {
          dang_set_error (error, "missing value argument #%u to %s()",
                          *expr_index_inout + 1, expr->function.name);
          return FALSE;
        }
      if (expr->function.args[*expr_index_inout]->type != DANG_EXPR_TYPE_VALUE)
        goto unexpected_expr_type;
      *expr_index_inout += 1;
      *pattern_used_out = 1;
      return TRUE;
    case 'T':
      if (*expr_index_inout == expr->function.n_args)
        {
          dang_set_error (error, "missing type argument #%u to %s()",
                          *expr_index_inout + 1, expr->function.name);
          return FALSE;
        }
      if (expr->function.args[*expr_index_inout]->type != DANG_EXPR_TYPE_VALUE
       || expr->function.args[*expr_index_inout]->value.type != dang_value_type_type ())
        {
          dang_set_error (error, "argument #%u to %s() not a type",
                          *expr_index_inout + 1, expr->function.name);
          return FALSE;
        }
      *expr_index_inout += 1;
      *pattern_used_out = 1;
      return TRUE;
    case 'I':
      if (*expr_index_inout == expr->function.n_args)
        {
          dang_set_error (error, "missing int argument #%u to %s()",
                          *expr_index_inout + 1, expr->function.name);
          return FALSE;
        }
      if (expr->function.args[*expr_index_inout]->type != DANG_EXPR_TYPE_VALUE
       || !is_stdint_type (expr->function.args[*expr_index_inout]->value.type))
        {
          dang_set_error (error, "argument #%u to %s() not an int",
                          *expr_index_inout + 1, expr->function.name);
          return FALSE;
        }
      *expr_index_inout += 1;
      *pattern_used_out = 1;
      return TRUE;

    case 0:
    case '|':
      {
        if (*expr_index_inout != expr->function.n_args)
          goto unexpected_expr_type;
        return TRUE;
      }
    case '*':
      {
        if (*expr_index_inout == expr->function.n_args)
          {
            const char *p = pattern + 1;
            skip_syntax_check (&p);
            *pattern_used_out = p - pattern;
            return TRUE;
          }
        while (*expr_index_inout < expr->function.n_args)
          {
            if (!syntax_check_mostly_nonrecursive (pattern + 1, pattern_used_out,
                                         expr, expr_index_inout,
                                         error))
              return FALSE;
          }
        *pattern_used_out += 1;
      }
      break;

    default:
      {
        /* Match function */
        const char *paren = strchr (pattern, '(');
        DangExpr *arg = expr->function.args[*expr_index_inout];
        assert (paren != NULL);
        assert (paren[1] == ')');
        if (arg->type != DANG_EXPR_TYPE_FUNCTION)
          goto unexpected_expr_type;
        if (memcmp (arg->function.name, pattern, paren - pattern) != 0
         || arg->function.name[paren - pattern] != 0)
          {
            dang_set_error (error, "got unexpected argument %s() #%u to %s()",
                            arg->function.name, *expr_index_inout+1,
                            expr->function.name);
            return FALSE;
          }
        *expr_index_inout += 1;
        *pattern_used_out = paren + 2 - pattern;
        return TRUE;
      }
    }
  return TRUE;


unexpected_expr_type:
  assert (*expr_index_inout < expr->function.n_args);
  dang_set_error (error, "got unexpected argument %s #%u to %s()",
                  dang_expr_type_name (expr->function.args[*expr_index_inout]->type),
                  *expr_index_inout + 1,
                  expr->function.name);
  return FALSE;
}

static dang_boolean
syntax_check_pattern (DangExpr *expr,
                      const char *syntax_check,
                      DangError **error)
{
  unsigned n_used = 0;
  unsigned expr_index = 0;
  while (syntax_check[n_used] && syntax_check[n_used] != '|')
    {
      unsigned sub_n_used;
      if (!syntax_check_mostly_nonrecursive (syntax_check + n_used, &sub_n_used,
                                             expr, &expr_index, error))
        {
          return FALSE;
        }
      n_used += sub_n_used;
    }
  if (expr_index < expr->function.n_args)
    {
      dang_set_error (error, "unexpected argument #%u to %s() ("DANG_CP_FORMAT")",
                      expr_index+1, expr->function.name,
                      DANG_CP_EXPR_ARGS (expr));
      return FALSE;
    }
  return TRUE;
}

static dang_boolean
syntax_check_alternation (DangExpr   *expr,
                          const char *pattern,
                        DangError **error)
{
  DangArray errors = DANG_ARRAY_STATIC_INIT (DangError *);
  DangError *e = NULL;
  for (;;)
    {
      const char *bar;
      if (syntax_check_pattern (expr, pattern, &e))
        {
          DangError **e = errors.data;
          unsigned i;
          for (i = 0; i < errors.len; i++)
            dang_error_unref (e[i]);
          dang_free (e);
          return TRUE;
        }
      dang_array_append (&errors, 1, &e);
      e = NULL;
      bar = strchr (pattern, '|');
      if (bar == NULL)
        {
          /* construct error */
          if (errors.len == 1)
            {
              *error = *(DangError**)errors.data;
              dang_array_clear (&errors);
              return FALSE;
            }
          else
            {
              /* laborious error-message prep */
              DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
              unsigned i;
              dang_string_buffer_printf (&buf, "%s", ((DangError**)errors.data)[0]->message);
              for (i = 1; i < errors.len; i++)
                {
                  e = ((DangError**)errors.data)[i];
                  dang_string_buffer_printf (&buf, ", %s", e->message);
                  dang_error_unref (e);
                }
              dang_set_error (error, "no branches of alternation succeeded: %s",
                              buf.str);
              dang_free (buf.str);
              dang_array_clear (&errors);
              return FALSE;
            }
        }
      pattern = bar + 1;
    }
}

static dang_boolean
syntax_check_recursive (DangExpr   *expr,
                        DangError **error)
{
  DangMetafunction *mf;
  unsigned i;
  if (expr->type != DANG_EXPR_TYPE_FUNCTION
   || expr->function.name[0] != '$')
    return TRUE;

  mf = dang_metafunction_lookup (expr->function.name);
  if (mf == NULL)
    {
      dang_set_error (error, "no metafunction %s() found", expr->function.name);
      return FALSE;
    }
  dang_assert (mf->syntax_check != NULL);
  if (!syntax_check_alternation (expr, mf->syntax_check, error))
    {
      dang_error_add_suffix (*error, "at "DANG_CP_FORMAT,
                             DANG_CP_EXPR_ARGS(expr));
      return FALSE;
    }

  for (i = 0; i < expr->function.n_args; i++)
    if (!syntax_check_recursive (expr->function.args[i], error))
      return FALSE;
  return TRUE;
}


  
dang_boolean dang_syntax_check (DangExpr *expr, DangError **error)
{
  return syntax_check_recursive (expr, error);
}
