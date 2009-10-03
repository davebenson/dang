/* questions:
      - some code is shared by typed-function invocation
 */
#include <string.h>
#include "dang.h"

static dang_boolean
function_params_equal (unsigned N,
                       DangFunctionParam *left,
                       DangFunctionParam *right)
{
  unsigned i;
  for (i = 0; i < N; i++)
    if (left[i].dir != right[i].dir
     || !dang_value_type_is_autocast (left[i].type, right[i].type))
      return FALSE;
  return TRUE;
}

dang_boolean
dang_untyped_function_make_stub_from_sig (DangUntypedFunction *untyped,
                                          DangSignature       *sig,
                                          DangError          **error)
{
  DangValueType *rt_expected, *rt_calculated;
  if (sig->n_params != untyped->n_params)
    {
      dang_set_error (error, "untyped function took %u args; signature demands %u args",
                      untyped->n_params, sig->n_params);
      return FALSE;
    }
  if (!dang_untyped_function_make_stub (untyped, sig->params, error))
    return FALSE;
  rt_expected = sig->return_type;
  rt_calculated = untyped->func->base.sig->return_type;
  if (!dang_value_type_is_autocast (rt_expected, rt_calculated))
    {
      dang_set_error (error, "untyped function returned %s, expected %s",
                      rt_expected ? rt_expected->full_name : "void",
                      rt_calculated ? rt_calculated->full_name : "void");
      return FALSE;
    }
  return TRUE;
}

/** dang_untyped_function_make_stub:
  * untyped:
  * input_types:
  *
  */ 
dang_boolean
dang_untyped_function_make_stub (DangUntypedFunction *untyped,
                                 DangFunctionParam   *params,
                                 DangError           **error)
{
  DangVarTable *var_table;
  DangAnnotations *annotations;
  dang_boolean has_rv;
  unsigned n_fparams;
  DangFunctionParam *fparams;
  unsigned at;
  unsigned i;
  DangSignature *sig;
  DangValueType *rv_type;
  DangUntypedFunctionReject *rej;
  DangUntypedFunctionReject **plastnext;
  DangUntypedFunctionFailure *failure;
  DangError *e = NULL;

  if (!dang_syntax_check (untyped->body, error))
    return FALSE;

  /* First see if we have this function (or an attempt at compiling it)
     around in our cache. */
  if (untyped->func != NULL
  && function_params_equal (untyped->n_params, params,
                            untyped->func->base.sig->params))
    return TRUE;
  plastnext = &untyped->rejects;
  for (rej = untyped->rejects; rej; rej = rej->next)
    {
      DangFunction *stub = rej->func;
      if (function_params_equal (untyped->n_params,
                                 params, stub->base.sig->params))
        {
          if (untyped->func)
            {
              /* swap */
              DangFunction *tmp = untyped->func;
              untyped->func = rej->func;
              rej->func = tmp;
            }
          else
            {
              /* remove from list */
              *plastnext = rej->next;
              untyped->func = rej->func;
              dang_free (rej);
            }
          return TRUE;
        }
      plastnext = &rej->next;
    }
  for (failure = untyped->failures; failure; failure = failure->next)
    {
      if (function_params_equal (untyped->n_params,
                                 params, (DangFunctionParam*)(failure+1)))
        {
          if (error)
            *error = dang_error_ref (failure->error);
          return FALSE;
        }
    }
  
  has_rv = TRUE;    /* XXX: assume, for the moment that untyped-functions are never returning void */

  var_table = dang_var_table_new (has_rv);
  annotations = dang_annotations_new ();

  /* Add input-types as expected by the recipient of this untyped function.
     ** how do we deal with void v non-void functions... [answer: we infer void from tag-type]
   */
  n_fparams = has_rv + untyped->n_params + untyped->n_closure_params;
  fparams = dang_newa (DangFunctionParam, n_fparams);
  at = 0;
  if (has_rv)
    {
      fparams[at].name = "return_value";
      fparams[at].dir = DANG_FUNCTION_PARAM_OUT;
      fparams[at].type = NULL;
      at++;
    }
  for (i = 0; i < untyped->n_params; i++)
    {
      fparams[at] = params[i];
      fparams[at].name = untyped->param_names[i];
      at++;
    }
  for (i = 0; i < untyped->n_closure_params; i++)
    {
      fparams[at].name = untyped->closure_params[i].name;
      fparams[at].dir = DANG_FUNCTION_PARAM_IN;
      fparams[at].type = untyped->closure_params[i].type;
      at++;
    }

  dang_assert (at == n_fparams);
  dang_var_table_add_params (var_table, NULL, n_fparams, fparams);

  if (!dang_expr_annotate_types (annotations, untyped->body,
                                 untyped->imports, var_table, &e))
    {
      dang_var_table_free (var_table);
      dang_annotations_free (annotations);

      goto failed;
    }

  /* Compute signature */

  /* Wrap the function in 
     'return' if we have not yet inferred a return-value. */
  if (has_rv)
    rv_type = dang_var_table_get_return_type (var_table);
  else
    rv_type = NULL;
  if (has_rv && rv_type == NULL)
    {
      DangExprTag *tag;
      tag = dang_expr_get_annotation (annotations, untyped->body, DANG_EXPR_ANNOTATION_TAG);
      dang_assert (tag != NULL);
      if (tag->tag_type == DANG_EXPR_TAG_VALUE)
        rv_type = tag->info.value.type;
      else if (tag->tag_type == DANG_EXPR_TAG_STATEMENT)
        rv_type = dang_value_type_void ();
      else if (tag->tag_type != DANG_EXPR_TAG_VALUE)
        {
          dang_var_table_free (var_table);
          dang_annotations_free (annotations);

          /* add to failure list */
          e = dang_error_new ("cannot get type of return-value for untyped-function at "DANG_CP_FORMAT,
                          DANG_CP_EXPR_ARGS (untyped->body));
          goto failed;
        }
      dang_var_table_set_type (var_table,
                               dang_var_table_get_return_var_id (var_table),
                               untyped->body,
                               rv_type);
    }


  sig = dang_signature_new (rv_type, n_fparams - has_rv, fparams + has_rv);

  if (untyped->func != NULL)
    {
      /* move to rejects list */
      DangUntypedFunctionReject *rej = dang_new (DangUntypedFunctionReject, 1);
      rej->next = untyped->rejects;
      rej->func = untyped->func;
      untyped->rejects = rej;
      untyped->func = NULL;
    }

  /* Create the stub function. */
  untyped->func = dang_function_new_stub (untyped->imports, sig,
                                          untyped->body,
                                          NULL, 0, NULL);
  dang_function_stub_set_annotations (untyped->func, annotations, var_table);
  dang_signature_unref (sig);
  return TRUE;


failed:
  /* add to failure list */
  failure = dang_malloc (sizeof(DangUntypedFunctionFailure)
                       + sizeof (DangFunctionParam) * untyped->n_params);
  memcpy (failure + 1, params, sizeof (DangFunctionParam) * untyped->n_params);
  failure->error = e;

  /* cheesy optimization? we should probably zero out the name,
     but we just don't use them. */

  failure->next = untyped->failures;
  untyped->failures = failure;

  if (error)
    *error = dang_error_ref (e);

  return FALSE;
}

void
dang_untyped_function_free (DangUntypedFunction *uf)
{
  unsigned i;
  dang_imports_unref (uf->imports);
  for (i = 0; i < uf->n_params; i++)
    dang_free (uf->param_names[i]);
  dang_free (uf->param_names);
  dang_expr_unref (uf->body);
  if (uf->func)
    dang_function_unref (uf->func);
  for (i = 0; i < uf->n_closure_params; i++)
    dang_free (uf->closure_params[i].name);
  while (uf->failures)
    {
      DangUntypedFunctionFailure *kill = uf->failures;
      uf->failures = kill->next;
      dang_error_unref (kill->error);
      dang_free (kill);
    }
  while (uf->rejects)
    {
      DangUntypedFunctionReject *kill = uf->rejects;
      uf->rejects = kill->next;
      dang_function_unref (kill->func);
      dang_free (kill);
    }
  dang_free (uf->closure_params);
  dang_free (uf);
}
