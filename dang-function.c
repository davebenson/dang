#include <string.h>
#include "dang.h"

const char *
dang_function_param_dir_name (DangFunctionParamDir dir)
{
  switch (dir)
    {
    case DANG_FUNCTION_PARAM_IN: return "in";
    case DANG_FUNCTION_PARAM_OUT: return "out";
    case DANG_FUNCTION_PARAM_INOUT: return "inout";
    }
  return "*bad dir*";
}

const char *dang_function_type_name (DangFunctionType type)
{
  switch (type)
    {
    case DANG_FUNCTION_TYPE_DANG: return "dang";
    case DANG_FUNCTION_TYPE_SIMPLE_C: return "simple-c";
    case DANG_FUNCTION_TYPE_C: return "c";
    case DANG_FUNCTION_TYPE_STUB: return "stub";
    case DANG_FUNCTION_TYPE_CLOSURE: return "closure";
    case DANG_FUNCTION_TYPE_NEW_OBJECT: return "new-object";
    }
  return "*bad function type*";
}

dang_boolean dang_catch_block_is_applicable (DangCatchBlock *catch_block,
                                             DangValueType  *thrown_type,
                                             DangCatchBlockClause **which_clause_out)
{
  unsigned i;
  for (i = 0; i < catch_block->n_clauses; i++)
    if (catch_block->clauses[i].type == thrown_type)
      {
        *which_clause_out = catch_block->clauses + i;
        return TRUE;
      }
  return FALSE;
}


static void
free_stack_info (DangFunctionStackInfo *stack_info)
{
  unsigned i;
  dang_free (stack_info->vars);
  dang_free (stack_info->params);
  for (i = 0; i < stack_info->n_catch_blocks; i++)
    dang_free (stack_info->catch_blocks[i].clauses);
  for (i = 0; i < stack_info->n_file_info; i++)
    {
      dang_string_unref (stack_info->file_info[i]->filename);
      dang_free (stack_info->file_info[i]);
    }
  dang_free (stack_info->file_info);
  dang_free (stack_info->catch_blocks);
  dang_free (stack_info);
}

void
dang_function_unref        (DangFunction    *function)
{
  //dang_warning ("dang_function_unref: %p: %u => %u", function, function->base.ref_count, function->base.ref_count - 1);
  if (--(function->base.ref_count) == 0)
    {
      switch (function->type)
        {
        case DANG_FUNCTION_TYPE_DANG:
          {
            unsigned i;
            for (i = 0; i < function->dang.n_destroy; i++)
              function->dang.destroy[i].func (function->dang.destroy[i].arg1,
                                              function->dang.destroy[i].arg2);
            dang_free (function->dang.destroy);
            dang_free (function->base.steps);
            break;
          }
        case DANG_FUNCTION_TYPE_SIMPLE_C:
          if (function->simple_c.func_data_destroy)
            function->simple_c.func_data_destroy (function->simple_c.func_data);
          dang_free (function->base.steps);
          break;
        case DANG_FUNCTION_TYPE_C:
          if (function->c.func_data_destroy)
            function->c.func_data_destroy (function->c.func_data);
          dang_free (function->c.arg_frame_offsets);
          dang_free (function->base.steps);
          break;
        case DANG_FUNCTION_TYPE_STUB:
          dang_expr_unref (function->stub.body);
          if (function->stub.imports)
            dang_imports_unref (function->stub.imports);
          if (function->stub.annotations)
            dang_annotations_free (function->stub.annotations);
          if (function->stub.var_table)
            dang_var_table_free (function->stub.var_table);
          break;
        case DANG_FUNCTION_TYPE_CLOSURE:
          _dang_closure_factory_destruct_closure_data (function->closure.factory, function);
          dang_closure_factory_unref (function->closure.factory);
          dang_function_unref (function->closure.underlying);
          break;
        case DANG_FUNCTION_TYPE_NEW_OBJECT:
          if (function->new_object.must_unref_constructor)
            dang_function_unref (function->new_object.constructor);
          break;
        default:
          dang_assert_not_reached ();
        }
      if (function->base.stack_info)
        free_stack_info (function->base.stack_info);
      dang_signature_unref (function->base.sig);
      dang_free (function);
    }
}

DangFunction *
dang_function_new_stub (DangImports *imports,
                        DangSignature *sig,
                        DangExpr *body,
                        DangValueType   *method_type,
                        unsigned         n_friends,
                        DangValueType  **friends)
{
  DangFunction *rv = dang_new0 (DangFunction, 1);
  rv->type = DANG_FUNCTION_TYPE_STUB;
  rv->base.sig = dang_signature_ref (sig);
  rv->base.ref_count = 1;
  rv->stub.body = dang_expr_ref (body);
  rv->stub.imports = dang_imports_ref (imports);
  rv->stub.method_type = method_type;
  rv->stub.n_friends = n_friends;
  rv->stub.friends = dang_memdup (friends, n_friends * sizeof (DangValueType*));
  rv->stub.var_table = NULL;
  rv->stub.annotations = NULL;
  return rv;
}

void
dang_function_stub_set_annotations (DangFunction *function,
                                    DangAnnotations *annotations,
                                    DangVarTable *var_table)
{
  dang_assert (function->stub.annotations == NULL);
  function->stub.annotations = annotations;
  function->stub.var_table = var_table;
}

DangFunction *dang_function_ref          (DangFunction    *function)
{
  //dang_warning ("dang_function_ref: %p: %u => %u", function, function->base.ref_count, function->base.ref_count + 1);
  ++(function->base.ref_count);
  return function;
}

DangFunction *dang_function_attach_ref          (DangFunction    *function)
{
  function->base.is_owned = TRUE;
  ++(function->base.ref_count);
  return function;
}
dang_boolean
dang_function_needs_registration (DangFunction *function)
{
  switch (function->type)
    {
    case DANG_FUNCTION_TYPE_STUB:
      return function->stub.cc == NULL;
    case DANG_FUNCTION_TYPE_NEW_OBJECT:
      return function->new_object.cc == NULL;
    default:
      return FALSE;
    }
}

dang_boolean
dang_function_get_code_position (DangFunction *function,
                                 DangStep     *step,
                                 DangCodePosition *out)
{
  DangFunctionStackInfo *stack_info = function->base.stack_info;
  DangFunctionLineInfo *line_infos;
  unsigned i, start, n;
  if (stack_info == NULL)
    return FALSE;
  if (stack_info->n_file_info == 0)
    return FALSE;
  if (step < stack_info->first_step)
    {
      dang_warning ("dang_function_get_code_position: step before first step");
      return FALSE;
    }
  if (step > stack_info->last_step)
    {
      dang_warning ("dang_function_get_code_position: step before last step");
      return FALSE;
    }

  /* Find the file.. there shouldn't be so many unique files in a function,
     so bear with a linear search (XXX: implement bsearch) */
  for (i = 0; i + 1 < stack_info->n_file_info; i++)
    if (stack_info->file_info[i+1]->line_infos[0].step > step)
      break;

  if (stack_info->file_info[i]->line_infos[0].step > step)
    return FALSE;

  /* Find the line in the file. */
  start = 0;
  n = stack_info->file_info[i]->n_line_infos;
  line_infos = stack_info->file_info[i]->line_infos;
  while (n > 1)
    {
      unsigned mid = start + n / 2;
      if (line_infos[mid].step == step)
        {
          start = mid;
          n = 1;
        }
      else if (line_infos[mid].step > step)
        {
          n /= 2;
        }
      else
        {
          n = (start + n - mid);
          start = mid;
        }
    }

  /* Initialize the code position */
  out->filename = dang_string_ref_copy (stack_info->file_info[i]->filename);
  out->line = line_infos[start].line;
  return TRUE;
}

dang_boolean
dang_function_call_nonyielding_v (DangFunction *function,
                                  void         *return_value,
                                  void        **arg_values,
                                  DangError   **error)
{
  if (function->type == DANG_FUNCTION_TYPE_SIMPLE_C)
    {
      if (!function->simple_c.func (arg_values, return_value,
                                    function->simple_c.func_data, error))
        return FALSE;
      return TRUE;
    }
  else if (function->type == DANG_FUNCTION_TYPE_STUB)
    {
      dang_set_error (error,
                      "cannot call nonyielding a stub function");
      return FALSE;
    }
  else
    {
      dang_boolean rv = FALSE;
      DangSignature *sig = function->base.sig;
      DangThread *thread = dang_thread_new (function, sig->n_params, arg_values);
      dang_thread_run (thread);
      switch (thread->status)
        {
        case DANG_THREAD_STATUS_THREW:
          if (thread->info.threw.type == dang_value_type_error ())
            *error = dang_error_ref (*(DangError**)thread->info.threw.value);
          else if (thread->info.threw.type != NULL)
            {
              char *str = dang_value_to_string (thread->info.threw.type,
                                                thread->info.threw.value);
              dang_set_error (error, "unhandled exception of type %s: %s",
                              thread->info.threw.type->full_name, str);
              dang_free (str);
            }
          else
            dang_set_error (error, "function threw no value");
          break;
        case DANG_THREAD_STATUS_DONE:
          {
            unsigned i, n_params = sig->n_params;
            unsigned offset = sizeof (DangThreadStackFrame);
            if (sig->return_type != NULL
             && sig->return_type != dang_value_type_void ())
              {
                DangValueType *rtype = sig->return_type;
                void *src;
                offset = DANG_ALIGN (offset, rtype->alignof_instance);
                src = (char*)thread->rv_frame + offset;
                if (rtype->init_assign)
                  rtype->init_assign (rtype, return_value, src);
                else
                  memcpy (return_value, src, rtype->sizeof_instance);
                offset += rtype->sizeof_instance;
              }
            for (i = 0; i < n_params; i++)
              {
                DangValueType *ptype = sig->params[i].type;
                offset = DANG_ALIGN (offset, ptype->alignof_instance);
                if (sig->params[i].dir == DANG_FUNCTION_PARAM_OUT
                 || sig->params[i].dir == DANG_FUNCTION_PARAM_INOUT)
                  {
                    DangValueAssignFunc f = sig->params[i].dir == DANG_FUNCTION_PARAM_OUT
                                          ? ptype->init_assign : ptype->assign;
                    void *src = (char*)thread->rv_frame + offset;
                    if (f == NULL)
                      memcpy (arg_values[i], src, ptype->sizeof_instance);
                    else
                      f (ptype, arg_values[i], src);
                  }
                offset += ptype->sizeof_instance;
              }
            rv = TRUE;
            break;
          }
        case DANG_THREAD_STATUS_YIELDED:
          dang_thread_cancel (thread);
          dang_set_error (error, "function yielded: not allowed");
          break;
        case DANG_THREAD_STATUS_CANCELLED:
          dang_set_error (error, "function was cancelled");
          break;
        default:
          dang_assert_not_reached ();
        }
      dang_thread_unref (thread);
      return rv;
    }
}
char *dang_function_to_string (DangFunction *func)
{
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  switch (func->type)
    {
    case DANG_FUNCTION_TYPE_DANG:
      dang_string_buffer_printf (&buf, "dang");
      break;
    case DANG_FUNCTION_TYPE_SIMPLE_C:
      {
        const char *name;
        DangNamespace *ns;
        if (
#if DANG_DEBUG
            !dang_debug_query_simple_c (func->simple_c.func, &ns, &name)
#else
            FALSE
#endif
           )
          dang_string_buffer_printf (&buf, "simple-c: unknown");
        else
          dang_string_buffer_printf (&buf, "simple-c: %s.%s",
                                     ns->full_name, name);
        break;
      }
    case DANG_FUNCTION_TYPE_STUB:
      dang_string_buffer_printf (&buf, "stub: "DANG_CP_FORMAT" (%p)",
                                 DANG_CP_EXPR_ARGS (func->stub.body),
                                 func);
      break;
    case DANG_FUNCTION_TYPE_CLOSURE:
      {
        char *und = dang_function_to_string (func->closure.underlying);
        dang_string_buffer_printf (&buf, "closure(%s)", und);
        dang_free (und);
        break;
      }
    case DANG_FUNCTION_TYPE_NEW_OBJECT:
      {
        dang_string_buffer_append (&buf, "new_object");
        break;
      }
    default:
      dang_assert_not_reached ();
    }
  dang_string_buffer_append (&buf, " ");
  dang_signature_dump (func->base.sig, &buf);
  return buf.str;
}
