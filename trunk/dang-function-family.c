#include "dang.h"

DangFunctionFamily *
dang_function_family_new (const char *name)
{
  DangFunctionFamily *rv = dang_new (DangFunctionFamily, 1);
  rv->ref_count = 1;
  rv->type = DANG_FUNCTION_FAMILY_CONTAINER;
  rv->name = dang_strdup (name);
  DANG_UTIL_ARRAY_INIT (&rv->info.container.families, DangFunctionFamily *);
  DANG_UTIL_ARRAY_INIT (&rv->info.container.functions, DangFunction *);
  return rv;
}
void                dang_function_family_container_add
                                             (DangFunctionFamily *container,
                                              DangFunctionFamily *subfamily)
{
  dang_assert (container->type == DANG_FUNCTION_FAMILY_CONTAINER);
  dang_function_family_ref (subfamily);
  dang_util_array_append (&container->info.container.families, 1, &subfamily);
}

void                dang_function_family_container_add_function
                                             (DangFunctionFamily *container,
                                              DangFunction       *function)
{
  dang_assert (container->type == DANG_FUNCTION_FAMILY_CONTAINER);
  dang_function_attach_ref (function);
  dang_util_array_append (&container->info.container.functions, 1, &function);
}

DangFunctionFamily *dang_function_family_new_variadic_c (const char *name,
                                                         DangFunctionTrySigFunc try_sig,
                                                       void *data,
                                                       DangDestroyNotify destroy)
{
  DangFunctionFamily *rv = dang_new (DangFunctionFamily, 1);
  rv->ref_count = 1;
  rv->type = DANG_FUNCTION_FAMILY_VARIADIC_C;
  rv->name = dang_strdup (name);
  rv->info.variadic_c.try_sig = try_sig;
  rv->info.variadic_c.data = data;
  rv->info.variadic_c.destroy = destroy;
  return rv;
}

DangFunction *dang_function_family_is_single (DangFunctionFamily *ff)
{
  if (ff->type == DANG_FUNCTION_FAMILY_CONTAINER
   && ff->info.container.families.len == 0
   && ff->info.container.functions.len == 1)
    return dang_function_ref (*((DangFunction**)ff->info.container.functions.data));
  return NULL;
}

DangFunctionFamily *
dang_function_family_new_template (const char         *name,
                                   DangImports        *imports,
                                   DangSignature      *sig,  /* must be templated */
                                   DangExpr           *body_expr,
                                   DangError         **error)
{
  DangFunctionFamily *rv;
  dang_assert (sig->is_templated);

  DANG_UNUSED (error);

  rv = dang_new (DangFunctionFamily, 1);
  rv->type = DANG_FUNCTION_FAMILY_TEMPLATE;
  rv->ref_count = 1;
  rv->name = dang_strdup (name);
  rv->info.templat.imports = dang_imports_ref (imports);
  rv->info.templat.sig = dang_signature_ref (sig);
      //unsigned n_tparams;
      //DangValueType **tparams;
  rv->info.templat.body_expr = dang_expr_ref (body_expr);
  rv->info.templat.method_type = NULL;
  rv->info.templat.n_friends = 0;
  rv->info.templat.friends = NULL;
  return rv;
}

DangFunctionFamily *dang_function_family_ref (DangFunctionFamily *ff)
{
  ++(ff->ref_count);
  return ff;
}

void                dang_function_family_unref (DangFunctionFamily *ff)
{
  if (--(ff->ref_count) == 0)
    {
      switch (ff->type)
        {
        case DANG_FUNCTION_FAMILY_CONTAINER:
          {
            unsigned i;
            DangFunction **subfunctions = ff->info.container.functions.data;
            DangFunctionFamily **subfamilies = ff->info.container.families.data;
            for (i = 0; i < ff->info.container.functions.len; i++)
              dang_function_unref (subfunctions[i]);
            for (i = 0; i < ff->info.container.families.len; i++)
              dang_function_family_unref (subfamilies[i]);
            dang_util_array_clear (&ff->info.container.functions);
            dang_util_array_clear (&ff->info.container.families);
            break;
          }
        case DANG_FUNCTION_FAMILY_VARIADIC_C:
          {
            if (ff->info.variadic_c.destroy)
              (*ff->info.variadic_c.destroy) (ff->info.variadic_c.data);
            break;
          }
        case DANG_FUNCTION_FAMILY_TEMPLATE:
          {
            if (ff->info.templat.imports)
              dang_imports_unref (ff->info.templat.imports);
            dang_signature_unref (ff->info.templat.sig);
            //dang_free (ff->info.templat.tparams);
            dang_expr_unref (ff->info.templat.body_expr);
            dang_free (ff->info.templat.friends);
            break;
          }
        default:
          dang_assert_not_reached ();
        }
      dang_free (ff->name);
      dang_free (ff);
    }
}

DangFunction       *
dang_function_family_try (DangFunctionFamily *ff,
                          DangMatchQuery     *mq,
                          DangError         **error)
{
  switch (ff->type)
    {
    case DANG_FUNCTION_FAMILY_CONTAINER:
      {
        unsigned i;
        for (i = 0; i < ff->info.container.functions.len; i++)
          {
            DangFunction *f;
            f = ((DangFunction**)ff->info.container.functions.data)[i];
            if (dang_signature_test (f->base.sig, mq))
              return dang_function_ref (f);
          }
        for (i = 0; i < ff->info.container.families.len; i++)
          {
            DangFunctionFamily *f;
            DangFunction *rv;
            f = ((DangFunctionFamily**)ff->info.container.families.data)[i];
            rv = dang_function_family_try (f, mq, error);
            if (rv)
              {
                /* Cache the new-found function in the container. */
                dang_function_ref (rv);
                dang_util_array_append (&ff->info.container.functions, 1, &rv);
                return rv;
              }
            if (error && *error)
              return NULL;
          }
        if (error)
          {
            /* --- error-handling --- */
            DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
            dang_string_buffer_printf (&buf, "  looking for %s", ff->name);
            dang_match_query_dump (mq, &buf);
            dang_string_buffer_printf (&buf, "\nExisting function prototypes are:\n");
            dang_function_family_dump (ff, ff->name, &buf);
            dang_set_error (error, "no function found for symbol %s:\n%s",
                            ff->name, buf.str);
            dang_free (buf.str);
          }
        return NULL;
      }
    case DANG_FUNCTION_FAMILY_VARIADIC_C:
      return ff->info.variadic_c.try_sig (mq, ff->info.variadic_c.data, error);
    case DANG_FUNCTION_FAMILY_TEMPLATE:
      {
        DangArray pairs = DANG_UTIL_ARRAY_STATIC_INIT (sizeof(DangValueType*));
        unsigned i;
        DangFunctionParam *concrete_params;
        DangExpr *real_body;
        if (!dang_signature_test_templated (ff->info.templat.sig, mq, &pairs))
          {
            dang_util_array_clear (&pairs);
            return NULL;
          }
        concrete_params = dang_newa (DangFunctionParam, ff->info.templat.sig->n_params);
        for (i = 0; i < mq->n_elements; i++)
          {
            concrete_params[i] = ff->info.templat.sig->params[i];
            concrete_params[i].type = dang_templated_type_make_concrete (ff->info.templat.sig->params[i].type, &pairs);
          }

        /* Substitute the expression. */
        real_body = dang_templated_expr_substitute_types (ff->info.templat.body_expr,
                                                          &pairs);
        dang_assert (real_body);

        /* Annotate */
        if (!dang_syntax_check (real_body, error))
          {
            dang_expr_unref (real_body);
            return NULL;
          }
        DangAnnotations *annotations;
        DangVarTable *var_table;
        dang_boolean has_rv;

        /* Try to substitute to get the type of the return value */
        has_rv = ff->info.templat.sig->return_type != NULL
              && ff->info.templat.sig->return_type != dang_value_type_void ();
        DangValueType *rv_type = NULL;
        DangValueType *constraint_type = NULL;
        if (has_rv)
          {
//            rv_type = dang_templated_type_make_concrete (ff->info.templat.sig->return_type,
//                                                         &pairs);
//            if (rv_type == NULL)
//              return NULL;
              constraint_type = ff->info.templat.sig->return_type;
//            if (rv_type->internals.is_templated)
              rv_type = NULL;
          }
        annotations = dang_annotations_new ();
        var_table = dang_var_table_new (has_rv);
        dang_var_table_add_params (var_table, rv_type, ff->info.templat.sig->n_params, concrete_params);
        if (!dang_expr_annotate_types (annotations, real_body, mq->imports, var_table, error))
          {
            dang_var_table_free (var_table);
            dang_annotations_free (annotations);
            dang_expr_unref (real_body);
            dang_util_array_clear (&pairs);
            return NULL;
          }

        /* ensure the return-type meets the template constraint */
        if (constraint_type != NULL)
          {
            DangValueType *real_rv_type = dang_var_table_get_return_type (var_table);
            if (!dang_templated_type_check (constraint_type, real_rv_type, &pairs))
              {
                dang_set_error (error, "type-mismatch: inferred function return-value type %s is not compatible with templated type %s ("DANG_CP_FORMAT")",
                                real_rv_type->full_name,
                                constraint_type->full_name,
                                DANG_CP_EXPR_ARGS (real_body));
                dang_var_table_free (var_table);
                dang_annotations_free (annotations);
                dang_util_array_clear (&pairs);
                dang_expr_unref (real_body);
                return NULL;
              }
            rv_type = real_rv_type;
          }

        /* make the stub function */
        DangSignature *real_sig;
        real_sig = dang_signature_new (rv_type, mq->n_elements, concrete_params);
        DangFunction *stub;
        stub = dang_function_new_stub (mq->imports, real_sig,
                                       real_body,
                                       NULL, 0, NULL    /* not a method */
                                      );
        dang_function_stub_set_annotations (stub, annotations, var_table);
        dang_signature_unref (real_sig);
        dang_util_array_clear (&pairs);
        dang_expr_unref (real_body);


        return stub;
      }

    default:
      dang_assert_not_reached ();
    }
}

void
dang_function_family_dump (DangFunctionFamily *family,
                           const char         *name,
                           DangStringBuffer   *buf)
{
  switch (family->type)
    {
    case DANG_FUNCTION_FAMILY_CONTAINER:
      {
        unsigned i = 0;
        DangFunction **functions = family->info.container.functions.data;
        DangFunctionFamily **families = family->info.container.families.data;
        for (i = 0; i < family->info.container.functions.len; i++)
          {
            dang_string_buffer_printf (buf, "  %s", name);
            dang_signature_dump (functions[i]->base.sig, buf);
            dang_string_buffer_append_c (buf, '\n');
          }
        for (i = 0; i < family->info.container.families.len; i++)
          {
            dang_function_family_dump (families[i], name, buf);
          }
        break;
      }
    case DANG_FUNCTION_FAMILY_VARIADIC_C:
      dang_string_buffer_printf (buf, "  %s(variadic)\n", name);
      break;
    case DANG_FUNCTION_FAMILY_TEMPLATE:
      dang_string_buffer_printf (buf, "  %s(template)\n", name);
      break;
    }
}

#define PARAM_DIRS_APPROX_EQUAL(dir1, dir2)                                   \
   (((dir1) == DANG_FUNCTION_PARAM_INOUT ? DANG_FUNCTION_PARAM_OUT : (dir1))  \
 == ((dir2) == DANG_FUNCTION_PARAM_INOUT ? DANG_FUNCTION_PARAM_OUT : (dir2)))


static dang_boolean
signatures_check_conflict (DangSignature *sig1,
                           DangSignature *sig2,
                           DangError    **error)
{
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  unsigned i;
  if (sig1->n_params != sig2->n_params)
    return TRUE;

  if (sig1->is_templated && !sig2->is_templated)
    return TRUE;
  if (!sig1->is_templated && sig2->is_templated)
    return TRUE;

  if (sig1->is_templated)
    {
      DangArray pairs = DANG_UTIL_ARRAY_STATIC_INIT (sizeof(DangValueType*));
      DangMatchQuery *q = dang_signature_make_match_query (sig2);
      if (dang_signature_test_templated (sig1, q, &pairs))
        {
          dang_util_array_set_size (&pairs, 0);
          dang_signature_match_query_free (q);
          q = dang_signature_make_match_query (sig1);
          if (dang_signature_test_templated (sig2, q, &pairs))
            {
              /* Identical/equivalent, so do not permit the addition. */
              dang_set_error (error, "signatures for templated types too close");
              dang_util_array_clear (&pairs);
              return FALSE;
            }
          dang_util_array_clear (&pairs);
        }
      dang_signature_match_query_free (q);
      return TRUE;
    }
  else
    {
      for (i = 0; i < sig1->n_params; i++)
        {
          if (sig1->params[i].type != sig2->params[i].type)
            return TRUE;
          if (!PARAM_DIRS_APPROX_EQUAL (sig1->params[i].dir,
                                        sig2->params[i].dir))
            return TRUE;
        }
    }
  dang_signature_dump (sig1, &buf);
  dang_set_error (error, "conflict with signatures: %s", buf.str);
  dang_free (buf.str);
  return FALSE;
}

dang_boolean
dang_function_family_check_conflict (DangFunctionFamily *family,
                                     DangSignature      *sig,
                                     DangError         **error)
{
  if (family->type == DANG_FUNCTION_FAMILY_CONTAINER)
    {
      DangFunction **functions = family->info.container.functions.data;
      DangFunctionFamily **families = family->info.container.families.data;
      unsigned i;
      for (i = 0; i < family->info.container.functions.len; i++)
        if (!signatures_check_conflict (functions[i]->base.sig, sig, error))
          return FALSE;
      for (i = 0; i < family->info.container.families.len; i++)
        if (!dang_function_family_check_conflict (families[i], sig, error))
          return FALSE;
    }
  return TRUE;
}

