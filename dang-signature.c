#include "string.h"
#include "dang.h"

const char *
dang_match_query_element_type_name (DangMatchQueryElementType type)
{
  switch (type)
    {
    case DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT:
      return "simple-input";
    case DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT:
      return "simple-input";
    case DANG_MATCH_QUERY_ELEMENT_FUNCTION_FAMILY:
      return "function-family";
    case DANG_MATCH_QUERY_ELEMENT_UNTYPED_FUNCTION:
      return "untyped-function";
    }
  return "*bad*element*type*";
}

dang_boolean
dang_function_param_parse (DangExpr *expr,
                           DangFunctionParam *out,
                           DangError **error)
{
  DangExpr **args;
  if (!dang_expr_is_function (expr, "$argument")
   || expr->function.n_args != 3)
    {
      dang_set_error (error,
                      "expected argument prototype to be $argument(_,_,_)");
      return FALSE;
    }
  args = expr->function.args;
  if (args[0]->type != DANG_EXPR_TYPE_BAREWORD)
    {
      dang_set_error (error,
                      "first argument to $argument must be a single bareword (one of: in out inout)");
      return FALSE;
    }
  if (args[2]->type != DANG_EXPR_TYPE_BAREWORD)
    {
      dang_set_error (error,
                      "third argument to $argument must be a single bareword (param name)");
      return FALSE;
    }
  if (strcmp (args[0]->bareword.name, "in") == 0)
    out->dir = DANG_FUNCTION_PARAM_IN;
  else if (strcmp (args[0]->bareword.name, "out") == 0)
    out->dir = DANG_FUNCTION_PARAM_OUT;
  else if (strcmp (args[0]->bareword.name, "inout") == 0)
    out->dir = DANG_FUNCTION_PARAM_INOUT;
  else
    {
      dang_set_error (error,
                      "first argument to $argument must be one of in,out,or inout (got '%s')",
                      args[0]->bareword.name);
      return FALSE;
    }
  if (args[1]->type != DANG_EXPR_TYPE_VALUE
   || args[1]->value.type != dang_value_type_type ())
    {
      dang_error_add_prefix (*error, "expected type in $argument");
      return FALSE;
    }
  out->type = * (DangValueType **) args[1]->value.value;
  out->name = args[2]->bareword.name;
  return TRUE;
}

DangSignature *
dang_signature_parse (DangExpr *args_sig_expr,
                      DangValueType *ret_type,
                      DangError **error)
{
  unsigned n_args = args_sig_expr->function.n_args;
  DangFunctionParam *args = dang_newa (DangFunctionParam, n_args);
  unsigned i;
  DangSignature *rv;
  for (i = 0; i < n_args; i++)
    {
      if (!dang_function_param_parse (args_sig_expr->function.args[i],
                                      args + i, error))
        {
          return NULL;
        }
    }
  rv = dang_signature_new (ret_type, n_args, args);
  return rv;
}

DangSignature *dang_signature_new          (DangValueType     *return_type,
                                            unsigned           n_params,
                                            DangFunctionParam *params)
{
  DangSignature *sig = dang_new (DangSignature, 1);
  unsigned i;
  dang_boolean is_templated = FALSE;
  sig->ref_count = 1;
  sig->return_type = return_type;
  sig->n_params = n_params;
  if (return_type && return_type->internals.is_templated)
    is_templated = TRUE;
  sig->params = dang_new (DangFunctionParam, n_params);
  for (i = 0; i < n_params; i++)
    {
      sig->params[i] = params[i];
      if (params[i].name)
        sig->params[i].name = dang_strdup (params[i].name);
      if (params[i].type->internals.is_templated)
        is_templated = TRUE;
    }
  sig->is_templated = is_templated;
  return sig;
}

DangSignature *dang_signature_ref          (DangSignature*sig)
{
  ++(sig->ref_count);
  return sig;
}
void           dang_signature_unref        (DangSignature*sig)
{
  if (--(sig->ref_count) == 0)
    {
      unsigned i;
      for (i = 0; i < sig->n_params; i++)
        dang_free ((char*)sig->params[i].name);
      dang_free (sig->params);
      dang_free (sig);
    }
}

/* peek at global void -> void signature */
static DangSignature void_sig = { 1, 0, NULL, NULL, FALSE };
DangSignature *dang_signature_void_func    (void)
{
  return &void_sig;
}

/* Resolve simple-input and simple-output match-query-elements first,
   since they are easiest to match. */
static dang_boolean
match_query_simple_element_test_param
                               (DangFunctionParam *param, /* may be templated */
                                DangMatchQueryElement *query,
                                DangUtilArray *pairs)
{
  switch (query->type)
    {
    case DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT:
      if (param->dir != DANG_FUNCTION_PARAM_IN)
        return FALSE;
      if (!param->type->internals.is_templated)
        {
          if (param->type != query->info.simple_input)
            return FALSE;
          return TRUE;
        }

      /* Handle templated match */
      if (!dang_templated_type_check (param->type, query->info.simple_input, pairs))
        return FALSE;
      return TRUE;
    case DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT:
      if (param->dir == DANG_FUNCTION_PARAM_IN)
        return FALSE;
      if (!param->type->internals.is_templated)
        {
          if (param->type != query->info.simple_output)
            return FALSE;
          return TRUE;
        }
      if (!dang_templated_type_check (param->type, query->info.simple_output, pairs))
        return FALSE;
      return TRUE;
    default:
      break;
    }
  dang_assert_not_reached ();
  return FALSE;
}
static dang_boolean
match_query_function_element_test_param
                               (DangFunctionParam *param, /* may be templated */
                                DangMatchQueryElement *query,
                                DangImports *imports,
                                DangUtilArray *pairs)
{
  /* Can we reduce the param to a concrete type?
     If not, bail. */
  DangValueType *ctype;
  DangSignature *sig;
  DangFunction *func;
  ctype = dang_templated_type_make_concrete (param->type, pairs);
  if (ctype == NULL)
    return FALSE;
  if (!dang_value_type_is_function (ctype))
    return FALSE;

  sig = ((DangValueTypeFunction*)ctype)->sig;
  switch (query->type)
    {
    case DANG_MATCH_QUERY_ELEMENT_FUNCTION_FAMILY:
      {
        DangMatchQuery subquery;
        DangMatchQueryElement *subelements;
        subquery.n_elements = sig->n_params;
        subelements = dang_newa (DangMatchQueryElement, sig->n_params);
        subquery.elements = subelements;
        subquery.imports = imports,
        func = dang_function_family_try (query->info.function_family,
                                         &subquery, NULL);
        if (func == NULL)
          return FALSE;
        dang_function_unref (func);     /* sigh, we'll look it up again... */
      }
      return TRUE;

    case DANG_MATCH_QUERY_ELEMENT_UNTYPED_FUNCTION:
      {
        DangValueType *ltype, *rtype;
        if (!dang_untyped_function_make_stub (query->info.untyped_function,
                                              sig->params, NULL))
          return FALSE;
        ltype = sig->return_type;
        rtype = query->info.untyped_function->func->base.sig->return_type;
        if (!dang_value_type_is_autocast (ltype, rtype))
          return FALSE;
        return TRUE;
      }

    default:
      dang_assert_not_reached ();
    }
  return FALSE;
}

dang_boolean
dang_signature_test_templated (DangSignature   *sig,
                               DangMatchQuery  *query,
                               DangUtilArray       *pairs_out)
{
  unsigned n_params;
  unsigned i;
  unsigned n_resolved = 0;
  dang_assert (sig->is_templated);
  if (sig->n_params != query->n_elements)
    return FALSE;
  n_params = sig->n_params;


  for (i = 0; i < sig->n_params; i++)
    if (query->elements[i].type == DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
     || query->elements[i].type == DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT)
      {
        if (!match_query_simple_element_test_param 
                                            (sig->params + i,
                                             query->elements + i,
                                             pairs_out))
          {
            return FALSE;
          }
        n_resolved++;
      }
  for (i = 0; i < sig->n_params; i++)
    if (query->elements[i].type == DANG_MATCH_QUERY_ELEMENT_FUNCTION_FAMILY
     || query->elements[i].type == DANG_MATCH_QUERY_ELEMENT_UNTYPED_FUNCTION)
      {
        if (!match_query_function_element_test_param 
                                            (sig->params + i,
                                             query->elements + i,
                                             query->imports,
                                             pairs_out))
          {
            return FALSE;
          }
        n_resolved++;
      }

  /* Ensure that we didn't miss a DangMatchQueryElementType
     between our two passes. */
  dang_assert (n_resolved == sig->n_params);

  return TRUE;
}

dang_boolean
dang_signature_test   (DangSignature *sig,
                       DangMatchQuery *query)
{
  dang_assert (!sig->is_templated);
  return dang_function_params_test (sig->n_params, sig->params, query);
}

dang_boolean
dang_function_params_test   (unsigned           n_params,
                             DangFunctionParam *params,
                             DangMatchQuery    *query)
{
  unsigned i;
  if (n_params != query->n_elements)
    return FALSE;
  for (i = 0; i < n_params; i++)
    {
      switch (query->elements[i].type)
        {
        case DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT:
          if (params[i].dir != DANG_FUNCTION_PARAM_IN)
            {
              dang_warning ("input parameter sent to %s parameter",
                            dang_function_param_dir_name (params[i].dir));
              return FALSE;
            }
          if (query->elements[i].info.simple_input != params[i].type)
            {
              //dang_warning ("arg type mismatch %s v %s",
                            //query->elements[i].type->full_name,
                            //params[i].type->full_name);
              return FALSE;
            }
          break;
        case DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT:
          if (params[i].dir != DANG_FUNCTION_PARAM_OUT
           && params[i].dir != DANG_FUNCTION_PARAM_INOUT)
            {
              dang_warning ("output parameter sent to in parameter");
              return FALSE;
            }
          if (query->elements[i].info.simple_output != params[i].type)
            {
              //dang_warning ("arg type mismatch %s v %s",
                            //query->elements[i].type->full_name,
                            //params[i].type->full_name);
              return FALSE;
            }
          break;

        case DANG_MATCH_QUERY_ELEMENT_UNTYPED_FUNCTION:
          {
            DangSignature *psig;
            DangUntypedFunction *uf;
            DangValueType *ltype, *rtype;
            if (params[i].dir != DANG_FUNCTION_PARAM_IN
             || !dang_value_type_is_function (params[i].type))
              {
                dang_warning ("untyped function sent to non-input function param");
                return FALSE;
              }
            psig = ((DangValueTypeFunction*)(params[i].type))->sig;
            uf = query->elements[i].info.untyped_function;
            if (psig->n_params != uf->n_params)
              {
                dang_warning ("untyped function took %u params, called-function's arg #%u requires %u params",
                              uf->n_params,
                              i+1,
                              psig->n_params);
                return FALSE;
              }
            /* try to make the stub */
            if (!dang_untyped_function_make_stub (uf, psig->params, NULL))
              return FALSE;
            ltype = psig->return_type;
            rtype = uf->func->base.sig->return_type;
            if (!dang_value_type_is_autocast (ltype, rtype))
              return FALSE;
          }
          break;

        case DANG_MATCH_QUERY_ELEMENT_FUNCTION_FAMILY:
          if (params[i].dir != DANG_FUNCTION_PARAM_IN
           || !dang_value_type_is_function (params[i].type))
            {
              dang_warning ("function family sent to non-input or non-function param");
              return FALSE;
            }
          break;

        default:
          dang_assert_not_reached ();
          return FALSE;
        }
    }
  return TRUE;
}

void
dang_match_query_dump (DangMatchQuery *query,
                       DangStringBuffer *buf)
{
  unsigned i;
  dang_string_buffer_printf (buf, "(");
  for (i = 0; i < query->n_elements; i++)
    switch (query->elements[i].type)
      {
      case DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT:
        dang_string_buffer_printf (buf,
                                   "%sin %s",
                                   i>0 ? ", " : "",
                                   query->elements[i].info.simple_input->full_name);
        break;
      case DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT:
        dang_string_buffer_printf (buf,
                                   "%sout %s",
                                   i>0 ? ", " : "",
                                   query->elements[i].info.simple_output->full_name);
        break;
      case DANG_MATCH_QUERY_ELEMENT_UNTYPED_FUNCTION:
        dang_string_buffer_printf (buf,
                                   "%suntyped function (%u params)",
                                   i>0 ? ", " : "",
                                   query->elements[i].info.untyped_function->n_params);
        break;
      case DANG_MATCH_QUERY_ELEMENT_FUNCTION_FAMILY:
        dang_string_buffer_printf (buf,
                                   "%sfunction-family: %p",
                                   i>0 ? ", " : "",
                                   query->elements[i].info.function_family);
        break;
    }
  dang_string_buffer_printf (buf, ")");
}

void
dang_signature_dump (DangSignature *sig,
                     DangStringBuffer *buf)
{
  unsigned i;
  dang_string_buffer_printf (buf, "(");
  for (i = 0; i < sig->n_params; i++)
    {
      dang_string_buffer_printf (buf,
                                 "%s%s %s",
                                 i>0 ? ", " : "",
                                 dang_function_param_dir_name (sig->params[i].dir),
                                 sig->params[i].type->full_name);
    }
  dang_string_buffer_printf (buf, ")");
}

dang_boolean
dang_signatures_equal (DangSignature *a,
                       DangSignature *b)
{
  unsigned i;
  if (a->n_params != b->n_params)
    return FALSE;
  for (i = 0; i < a->n_params; i++)
    {
      if (a->params[i].dir != b->params[i].dir)
        return FALSE;
      if (a->params[i].type != b->params[i].type)
        return FALSE;
    }
  return (a->return_type==dang_value_type_void() ? NULL : a->return_type)
     ==  (b->return_type==dang_value_type_void() ? NULL : b->return_type);
}

DangMatchQuery *
dang_signature_make_match_query (DangSignature *sig)
{
  unsigned size = sizeof (DangMatchQuery)
                + sig->n_params * sizeof (DangMatchQueryElement);
  DangMatchQuery *mq;
  unsigned i;
  //...
  mq = dang_malloc (size);
  mq->n_elements = sig->n_params;
  mq->elements = (DangMatchQueryElement*)(mq+1);
  for (i = 0; i < sig->n_params; i++)
    if (sig->params[i].dir == DANG_FUNCTION_PARAM_IN)
      {
        mq->elements[i].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
        mq->elements[i].info.simple_input = sig->params[i].type;
      }
    else
      {
        mq->elements[i].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT;
        mq->elements[i].info.simple_output = sig->params[i].type;
      }
  return mq;
}
