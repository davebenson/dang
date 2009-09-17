#include "dang.h"
#include "gskrbtreemacros.h"



DangCompileContext *dang_compile_context_new (void)
{
  DangCompileContext *rv = dang_new (DangCompileContext, 1);
  DANG_ARRAY_INIT (&rv->stubs, DangFunction *);
  DANG_ARRAY_INIT (&rv->new_object_functions, DangFunction *);
  rv->finishing = FALSE;
  return rv;
}
void dang_compile_context_free (DangCompileContext *cc)
{
  unsigned i;
  for (i = 0; i < cc->stubs.len; i++)
    {
      DangFunction *function = ((DangFunction**)cc->stubs.data)[i];
      dang_function_unref (function);
    }
  for (i = 0; i < cc->new_object_functions.len; i++)
    {
      DangFunction *function = ((DangFunction**)cc->new_object_functions.data)[i];
      dang_function_unref (function);
    }
  dang_array_clear (&cc->stubs);
  dang_array_clear (&cc->new_object_functions);
  dang_free (cc);
}

void                dang_compile_context_register(DangCompileContext *cc,
                                                  DangFunction       *function)
{
  switch (function->type)
    {
    case DANG_FUNCTION_TYPE_STUB:
      dang_function_ref (function);
      function->stub.cc = cc;
      dang_array_append (&cc->stubs, 1, &function);
      break;
    case DANG_FUNCTION_TYPE_NEW_OBJECT:
      dang_function_ref (function);
      function->new_object.cc = cc;
      dang_array_append (&cc->new_object_functions, 1, &function);
      if (dang_function_needs_registration (function->new_object.constructor))
        dang_compile_context_register (cc, function->new_object.constructor);
      break;
    default:
      dang_assert_not_reached ();
    }
}

dang_boolean
dang_compile_context_finish (DangCompileContext *cc,
                             DangError **error)
{
  unsigned i;
  dang_assert (!cc->finishing);
  cc->finishing = TRUE;

  for (i = 0; i < cc->stubs.len; i++)
    {
      DangFunction *function = ((DangFunction**)cc->stubs.data)[i];
      DangBuilder *builder;
      DangCompileResult res;
      DangExpr *body;
      DangSignature *sig = function->base.sig;
      DangVarTable *var_table;
      DangAnnotations *annotations;
      unsigned fr;
      DangCompileFlags flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;

      if (function->stub.annotations)
        {
          annotations = function->stub.annotations;
          var_table = function->stub.var_table;
          function->stub.annotations = NULL;
          function->stub.var_table = NULL;
        }
      else
        {
          annotations = dang_annotations_new ();
          var_table = dang_var_table_new (sig->return_type != NULL
                                          && sig->return_type != dang_value_type_void ());
          dang_var_table_add_params (var_table, sig->return_type,
                                     sig->n_params, sig->params);

          if (!dang_syntax_check (function->stub.body, error))
            return FALSE;

          if (!dang_expr_annotate_types (annotations, function->stub.body, function->stub.imports, var_table, error))
            {
              dang_var_table_free (var_table);
              dang_annotations_free (annotations);
              return FALSE;
            }

          /* If the function body is an $untyped_function, infer its type */
          if (dang_expr_is_function (function->stub.body, "$untyped_function"))
            {
              DangValueTypeFunction *ftype = (DangValueTypeFunction*) sig->return_type;
              DangExprTag *ut;
              if (sig->return_type == NULL
               || !dang_value_type_is_function (sig->return_type))
                {
                  dang_set_error (error,
                                  "got untyped-function expression in function returning %s ("DANG_CP_FORMAT")",
                                  sig->return_type == NULL ? "void" : sig->return_type->full_name,
                                  DANG_CP_EXPR_ARGS (function->stub.body));
                  return FALSE;
                }
              ut = dang_expr_get_annotation (annotations, function->stub.body,
                                             DANG_EXPR_ANNOTATION_TAG);
              dang_assert (ut->tag_type == DANG_EXPR_TAG_UNTYPED_FUNCTION);
              if (!dang_untyped_function_make_stub_from_sig
                      (ut->info.untyped_function, ftype->sig, error))
                {
                  dang_error_add_pos_suffix (*error, &function->stub.body->any.code_position);
                  return FALSE;
                }
            }
        }

      builder = dang_builder_new (function, var_table, annotations);

      if (function->stub.method_type != NULL)
        {
          DangValueType *t = function->stub.method_type;
          dang_builder_push_friend (builder, t, TRUE);
          t = t->internals.parent;
          while (t)
            {
              dang_builder_push_friend (builder, t, FALSE);
              t = t->internals.parent;
            }
        }
      for (fr = 0; fr < function->stub.n_friends; fr++)
        dang_builder_push_friend (builder, function->stub.friends[fr], TRUE);

      body = dang_expr_ref (function->stub.body);
      dang_compile (function->stub.body, builder, &flags, &res);
      if (res.type == DANG_COMPILE_RESULT_ERROR)
        {
          *error = res.error.error;
          goto got_error;
        }
      if (res.type != DANG_COMPILE_RESULT_VOID
       && (sig->return_type != NULL
           && sig->return_type != dang_value_type_void ()))
        {
          /* Should we return the value? */
          if (res.any.return_type != sig->return_type)
            {
              dang_set_error (error, "type mismatch, function expected to return %s, returned %s ("DANG_CP_FORMAT")",
                              sig->return_type->full_name,
                              res.any.return_type->full_name,
                              DANG_CP_EXPR_ARGS (body));
              dang_compile_result_clear (&res, builder);
              goto got_error;
            }
          if (res.type != DANG_COMPILE_RESULT_STACK
           || res.stack.var_id != 0)
            {
              DangCompileResult ret_var;
              dang_compile_result_init_stack (&ret_var, sig->return_type,
                                              0, TRUE, TRUE, FALSE);
              dang_builder_add_assign (builder, &ret_var, &res);
            }
          dang_builder_add_return (builder);
        }
      dang_compile_result_clear (&res, builder);
      if (!dang_builder_compile (builder, error))
        {
          goto got_error;
        }

      /* erase annotations */
      dang_expr_unref (body);

      // TODO
      dang_var_table_free (var_table);

#ifdef DANG_DEBUG
//      if (dang_debug_disassemble)
//        dang_debug_dump_function (function);
#endif
      /* the builder is destroyed by dang_builder_compile() */
      dang_function_unref (function);
      dang_annotations_free (annotations);
      continue;         /* go on with the loop */

got_error:
      dang_array_remove (&cc->stubs, 0, i);
      cc->finishing = FALSE;
      dang_builder_destroy (builder);
      dang_expr_unref (body);
      dang_annotations_free (annotations);
      dang_var_table_free (var_table);
      return FALSE;
    }
  for (i = 0; i < cc->new_object_functions.len; i++)
    {
      DangFunction *no = ((DangFunction**) cc->new_object_functions.data)[i];
      dang_assert (no->type == DANG_FUNCTION_TYPE_NEW_OBJECT);
      no->base.frame_size = no->new_object.constructor->base.frame_size;
      dang_function_unref (no);
    }

  dang_array_set_size (&cc->new_object_functions, 0);
  dang_array_set_size (&cc->stubs, 0);
  cc->finishing = FALSE;
  return TRUE;
}
