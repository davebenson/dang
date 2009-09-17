#include <string.h>
#include "dang.h"

void
dang_compile_literal_function_invocation (DangFunction        *function,
                                          DangBuilder *builder,
                                          DangCompileResult   *return_value_info,
                                          unsigned             n_params,
                                          DangCompileResult   *params)
{
  DangValueType *ftype = dang_value_type_function (function->base.sig);
  DangCompileResult res;
  dang_compile_result_init_literal (&res, ftype, &function);
  dang_compile_function_invocation (&res, builder, return_value_info, n_params, params);
  dang_compile_result_clear (&res, builder);
}

void
dang_compile_function_invocation (DangCompileResult   *function,
                                  DangBuilder *builder,
                                  DangCompileResult   *return_value_info,
                                  unsigned             n_params,
                                  DangCompileResult   *params)
{
  DangSignature *sig;
  DangInsn insn;
  unsigned i, out = 0, n_par;
  DangInsnValue *par;
  /* If it's a constant function, see if has a specialized 'compile' method */
  if (function->type == DANG_COMPILE_RESULT_LITERAL)
    {
      DangFunction *f = * (DangFunction **) function->literal.value;
      if (f->base.compile != NULL)
        {
          dang_assert (f != NULL);
          f->base.compile (f, builder, return_value_info, n_params, params);
          return;
        }
      if (dang_function_needs_registration (f))
        dang_compile_context_register (builder->function->stub.cc, f);
    }
  else if (function->type != DANG_COMPILE_RESULT_STACK)
    {
      DangCompileFlags flags = DANG_COMPILE_FLAGS_RVALUE_RESTRICTIVE;
      dang_compile_obey_flags (builder, &flags, function);
    }

  /* warmups.  make sure all output parameters are initialized
     (in the caller frame) */
  for (i = 0; i < n_params; i++)
    if (params[i].type == DANG_COMPILE_RESULT_STACK
      && !params[i].stack.was_initialized)
      dang_compile_result_force_initialize (builder, params + i);
  if (return_value_info != NULL
      && return_value_info->type == DANG_COMPILE_RESULT_STACK
      && !return_value_info->stack.was_initialized)
    dang_compile_result_force_initialize (builder, return_value_info);


  /* Otherwise, add a FUNCTION_CALL insn */
  sig = ((DangValueTypeFunction*)function->any.return_type)->sig;
  dang_assert (sig->n_params == n_params);
  n_par = (return_value_info ? 1 : 0) + n_params;
  par = dang_new (DangInsnValue, n_par);
  if (return_value_info)
    {
      dang_assert (sig->return_type != NULL && sig->return_type != dang_value_type_void ());
      dang_insn_value_from_compile_result (&par[out++], return_value_info);
    }
  else
    dang_assert (sig->return_type == NULL || sig->return_type == dang_value_type_void ());
  for (i = 0; i < n_params; i++)
    dang_insn_value_from_compile_result (&par[out++], params + i);
  dang_insn_init (&insn, DANG_INSN_TYPE_FUNCTION_CALL);
  insn.function_call.sig = dang_signature_ref (sig);
  dang_insn_value_from_compile_result (&insn.function_call.function, function);
  insn.function_call.params = par;
  DangVarId frame_var_id;
  frame_var_id = dang_builder_add_tmp (builder, dang_value_type_reserved_pointer ());
  insn.function_call.frame_var_id = frame_var_id;
  dang_builder_note_var_create (builder, frame_var_id);
  dang_builder_note_var_destruct (builder, frame_var_id);
  dang_builder_add_insn (builder, &insn);

  /* Any "stack_plus" compile-results should be tripped now */
  if (return_value_info
   && return_value_info->type == DANG_COMPILE_RESULT_STACK
   && return_value_info->stack.lvalue_callback != NULL)
    {
      return_value_info->stack.lvalue_callback (return_value_info,
                                                builder);
    }
  for (i = 0; i < n_params; i++)
    if (params[i].type == DANG_COMPILE_RESULT_STACK
      && params[i].stack.lvalue_callback != NULL)
      {
        dang_assert (sig->params[i].dir == DANG_FUNCTION_PARAM_INOUT
                  || sig->params[i].dir == DANG_FUNCTION_PARAM_OUT);
        params[i].stack.lvalue_callback (params + i, builder);
      }
}

