#include <string.h>
#include "dang.h"
#include "magic.h"
#include "gskrbtreemacros.h"


static DangValueTypeTemplateParam *by_name_tree;
#define COMPARE_TEMPLATE_PARAMS(a,b, rv) \
  rv = strcmp (a->formal_name, b->formal_name)
#define COMPARE_STRING_TO_TEMPLATE_PARAM(a,b, rv) \
  rv = strcmp (a, b->formal_name)
#define GET_TEMPLATE_PARAM_TREE() \
  by_name_tree, DangValueTypeTemplateParam *, \
  GSK_STD_GET_IS_RED, GSK_STD_SET_IS_RED, \
  parent, left, right, COMPARE_TEMPLATE_PARAMS


DangValueType *dang_value_type_template_param (const char *name)
{
  DangValueTypeTemplateParam *out;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_TEMPLATE_PARAM_TREE (), name,
                                COMPARE_STRING_TO_TEMPLATE_PARAM, out);
  if (out == NULL)
    {
      DangValueTypeTemplateParam *conflict;
      out = dang_new0 (DangValueTypeTemplateParam, 1);
      out->base_type.magic = DANG_VALUE_TYPE_MAGIC;
      out->base_type.ref_count = 1;
      out->base_type.full_name = dang_strdup_printf ("%%%%%s", name);
      out->base_type.sizeof_instance = (unsigned)-1;
      out->base_type.alignof_instance = (unsigned)-2;
      out->base_type.internals.is_templated = TRUE;
      out->formal_name = dang_strdup (name);
      GSK_RBTREE_INSERT (GET_TEMPLATE_PARAM_TREE (), out, conflict);
      dang_assert (conflict == NULL);
    }
  return (DangValueType *) out;
}

dang_boolean
dang_value_type_is_template_param (DangValueType *type)
{
  return (type->sizeof_instance == (unsigned)-1
       && type->alignof_instance == (unsigned)-2);
}

void
dang_type_gather_template_params (DangValueType *type,
                                  DangUtilArray     *params_out)
{
  if (dang_value_type_is_template_param (type))
    {
      unsigned len = params_out->len;
      DangValueType **types = params_out->data;
      unsigned i;
      for (i = 0; i < len; i++)
        if (types[i] == type)
          return;
      dang_util_array_append (params_out, 1, &type);
      return;
    }
  if (dang_value_type_is_tensor (type))
    {
      DangValueTypeTensor *ttype = (DangValueTypeTensor *) type;
      dang_type_gather_template_params (ttype->element_type, params_out);
      return;
    }
  if (dang_value_type_is_function (type))
    {
      DangSignature *sig = ((DangValueTypeFunction *) type)->sig;
      unsigned i;
      if (sig->return_type)
        dang_type_gather_template_params (sig->return_type, params_out);
      for (i = 0; i < sig->n_params; i++)
        dang_type_gather_template_params (sig->params[i].type, params_out);
      return;
    }

  /* ignore all other type, for now */
  return;
}

dang_boolean dang_expr_contains_disallowed_template_param (DangExpr *expr,
                                                           unsigned n_allowed,
                                                           DangValueType **allowed,
                                                           const char **bad_name_out,
                                                           DangExpr **bad_expr_out)
{
  switch (expr->type)
    {
    case DANG_EXPR_TYPE_VALUE:
      if (expr->value.type != dang_value_type_type ())
        return FALSE;
      if (!dang_value_type_contains_disallowed_template_param
            (*(DangValueType**)expr->value.value, n_allowed, allowed, bad_name_out))
        return FALSE;
      *bad_expr_out = expr;
      return TRUE;
    case DANG_EXPR_TYPE_FUNCTION:
      {
        unsigned i;
        for (i = 0 ; i < expr->function.n_args; i++)
          if (dang_expr_contains_disallowed_template_param (expr->function.args[i],
                                                            n_allowed, allowed,
                                                            bad_name_out, bad_expr_out))
            return TRUE;
        return FALSE;
      }

    case DANG_EXPR_TYPE_BAREWORD:
      return FALSE;
    }
  dang_assert_not_reached ();
}


dang_boolean
dang_value_type_contains_disallowed_template_param (DangValueType *type,
                                                    unsigned n_allowed,
                                                    DangValueType **allowed,
                                                    const char **bad_name_out)
{
  if (!type->internals.is_templated)
    return FALSE;

  if (dang_value_type_is_template_param (type))
    {
      unsigned i;
      for (i = 0; i < n_allowed; i++)
        {
          if (allowed[i] == type)
            return FALSE;
        }
      if (bad_name_out)
        *bad_name_out = ((DangValueTypeTemplateParam*)type)->formal_name;
      return TRUE;
    }
  if (dang_value_type_is_tensor (type))
    {
      return dang_value_type_contains_disallowed_template_param
                    (((DangValueTypeTensor*)type)->element_type,
                     n_allowed, allowed, bad_name_out);
    }
  if (dang_value_type_is_array (type))
    {
      return dang_value_type_contains_disallowed_template_param
                    (((DangValueTypeArray*)type)->element_type,
                     n_allowed, allowed, bad_name_out);
    }
  if (dang_value_type_is_function (type))
    {
      DangSignature *sig = ((DangValueTypeFunction*)type)->sig;
      unsigned i;
      if (dang_value_type_contains_disallowed_template_param
                     (sig->return_type, n_allowed, allowed, bad_name_out))
        return TRUE;
      for (i = 0; i < sig->n_params; i++)
        if (dang_value_type_contains_disallowed_template_param
                       (sig->params[i].type, n_allowed, allowed, bad_name_out))
          return TRUE;
      return FALSE;
    }

  dang_die ("unknown templated type %s", type->full_name);
  return FALSE;
}
dang_boolean dang_templated_type_check (DangValueType *templated_type,
                                        DangValueType *match_type,
                                        DangUtilArray     *pairs_out)
{
  unsigned n_pairs = pairs_out->len / 2;
  DangValueType **pairs = pairs_out->data;
  if (!templated_type->internals.is_templated)
    return match_type == templated_type;
  if (dang_value_type_is_template_param (templated_type))
    {
      unsigned i;
      for (i = 0; i < n_pairs; i++)
        if (pairs[2*i] == templated_type)
          {
            if (pairs[2*i+1] != match_type)
              return FALSE;
            return TRUE;
          }
      dang_util_array_append (pairs_out, 1, &templated_type);
      dang_util_array_append (pairs_out, 1, &match_type);
      return TRUE;
    }
  if (dang_value_type_is_tensor (templated_type))
    {
      DangValueTypeTensor *ta = (DangValueTypeTensor *) templated_type;
      DangValueTypeTensor *tb;
      if (!dang_value_type_is_tensor (match_type))
        return FALSE;
      tb = (DangValueTypeTensor *) match_type;
      if (ta->rank != tb->rank)
        return FALSE;
      if (!dang_templated_type_check (ta->element_type,
                                      tb->element_type,
                                      pairs_out))
        return FALSE;
      return TRUE;
    }
  if (dang_value_type_is_function (templated_type))
    {
      DangValueTypeFunction *fa = (DangValueTypeFunction*) templated_type;
      DangValueTypeFunction *fb;
      DangValueType *at, *bt;
      unsigned i;
      if (!dang_value_type_is_function (match_type))
        return FALSE;
      fb = (DangValueTypeFunction *) match_type;
      if (fa->sig->n_params != fb->sig->n_params)
        return FALSE;
      at = fa->sig->return_type ? fa->sig->return_type : dang_value_type_void ();
      bt = fb->sig->return_type ? fb->sig->return_type : dang_value_type_void ();
      if (at != bt)
        {
          if (!dang_templated_type_check (at, bt, pairs_out))
            return FALSE;
        }
      for (i = 0; i < fa->sig->n_params; i++)
        {
          if (fa->sig->params[i].dir != fb->sig->params[i].dir)
            return FALSE;
          if (!dang_templated_type_check (fa->sig->params[i].type,
                                          fb->sig->params[i].type,
                                          pairs_out))
            return FALSE;
        }
      return TRUE;
    }

  dang_assert_not_reached ();
  return FALSE;
}

DangExpr *
dang_templated_expr_substitute_types (DangExpr  *orig,
                                      DangUtilArray *pairs)
{
  switch (orig->type)
    {
    case DANG_EXPR_TYPE_VALUE:
      if (orig->value.type == dang_value_type_type ())
        {
          DangValueType *ot = * (DangValueType**) orig->value.value;
          DangValueType *t = dang_templated_type_make_concrete (ot, pairs);
          DangExpr *rv = dang_expr_new_value (dang_value_type_type (), &t);
          dang_code_position_copy (&rv->any.code_position,
                                   &orig->any.code_position);
          return rv;
        }
      else
        return dang_expr_ref (orig);
    case DANG_EXPR_TYPE_FUNCTION:
      {
        DangExpr **e = dang_new (DangExpr *, orig->function.n_args);
        dang_boolean changed = FALSE;
        unsigned i;
        for (i = 0; i < orig->function.n_args; i++)
          {
            e[i] = dang_templated_expr_substitute_types (orig->function.args[i], pairs);
            if (e[i] != orig->function.args[i])
              changed = TRUE;
          }
        if (changed)
          {
            DangExpr *rv;
            rv = dang_expr_new_function_take (orig->function.name,
                                              orig->function.n_args, e);
            dang_code_position_copy (&rv->any.code_position,
                                     &orig->any.code_position);
            dang_free (e);
            return rv;
          }
        for (i = 0; i < orig->function.n_args; i++)
          dang_expr_unref (e[i]);
        dang_free (e);
        return dang_expr_ref (orig);
      }
    case DANG_EXPR_TYPE_BAREWORD:
      return dang_expr_ref (orig);
    }
  dang_assert_not_reached ();
  return 0;
}

DangValueType *dang_templated_type_make_concrete (DangValueType *templated_type,
                                                  DangUtilArray *tt_pairs)
{
  unsigned n_pairs = tt_pairs->len / 2;
  DangValueType **pairs = tt_pairs->data;
  unsigned i;
  if (!templated_type->internals.is_templated)
    return templated_type;


  if (dang_value_type_is_template_param (templated_type))
    {
      for (i = 0; i < n_pairs; i++)
        if (pairs[2*i] == templated_type)
          return pairs[2*i+1];
      return NULL;
    }
  if (dang_value_type_is_tensor (templated_type))
    {
      DangValueTypeTensor *ttype = (DangValueTypeTensor *) templated_type;
      DangValueType *new_elt_type;
      new_elt_type = dang_templated_type_make_concrete (ttype->element_type, tt_pairs);
      return dang_value_type_tensor (new_elt_type, ttype->rank);
    }
  if (dang_value_type_is_array (templated_type))
    {
      DangValueTypeArray *atype = (DangValueTypeArray *) templated_type;
      DangValueType *new_elt_type;
      new_elt_type = dang_templated_type_make_concrete (atype->element_type, tt_pairs);
      return dang_value_type_array (new_elt_type, atype->rank);
    }
  if (dang_value_type_is_function (templated_type))
    {
      DangValueTypeFunction *ftype = (DangValueTypeFunction *) templated_type;
      DangValueType *rv_type = ftype->sig->return_type
                             ? dang_templated_type_make_concrete (ftype->sig->return_type, tt_pairs)
                             : NULL;
      DangValueType *rv;
      DangFunctionParam *params = dang_newa (DangFunctionParam, ftype->sig->n_params);
      DangSignature *sig;
      memcpy (params, ftype->sig->params, ftype->sig->n_params * sizeof (DangFunctionParam));
      for (i = 0; i < ftype->sig->n_params; i++)
        params[i].type = dang_templated_type_make_concrete (params[i].type, tt_pairs);
      sig = dang_signature_new (rv_type, ftype->sig->n_params, params);
      rv = dang_value_type_function (sig);
      dang_signature_unref (sig);
      return rv;
    }
  return NULL;
}


static void
free_template_param_tree_recursive (DangValueTypeTemplateParam *p)
{
  if (p == NULL)
    return;
  free_template_param_tree_recursive (p->left);
  free_template_param_tree_recursive (p->right);
  dang_free (p->formal_name);
  dang_free (p->base_type.full_name);
  dang_free (p);
}
void _dang_template_cleanup (void)
{
  free_template_param_tree_recursive (by_name_tree);
  by_name_tree = NULL;
}
