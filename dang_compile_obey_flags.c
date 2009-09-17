#include <string.h>
#include "dang.h"


void
dang_compile_result_force_initialize (DangBuilder *builder,
                                      DangCompileResult   *result_inout)
{
  DangInsn insn;
  switch (result_inout->type)
    {
    case DANG_COMPILE_RESULT_STACK:
      if (result_inout->stack.was_initialized)
        return;
      result_inout->stack.was_initialized = TRUE;
      break;
    default:
      return;
    }

  dang_insn_init (&insn, DANG_INSN_TYPE_INIT);
  insn.init.var = result_inout->stack.var_id;
  dang_builder_note_var_create (builder, result_inout->stack.var_id);
  dang_builder_add_insn (builder, &insn);
}

void dang_compile_obey_flags (DangBuilder *builder,
                              DangCompileFlags    *flags,
                              DangCompileResult   *result_inout)
{
  if (flags->permit_void && result_inout->type == DANG_COMPILE_RESULT_VOID)
    return;
  if (flags->must_be_lvalue && !result_inout->any.is_lvalue)
    dang_warning ("dang_compile_obey_flags: must_be_lvalue but wasn't");
  if (flags->must_be_rvalue && !result_inout->any.is_rvalue)
    dang_warning ("dang_compile_obey_flags: must_be_rvalue but wasn't");
  switch (result_inout->type)
    {
    case DANG_COMPILE_RESULT_STACK:
      if (!result_inout->stack.was_initialized && !flags->permit_uninitialized)
        dang_compile_result_force_initialize (builder, result_inout);
    case DANG_COMPILE_RESULT_VOID:
    case DANG_COMPILE_RESULT_ERROR:
      return;
    case DANG_COMPILE_RESULT_GLOBAL:
      if (flags->permit_global)
        return;
      break;
    case DANG_COMPILE_RESULT_POINTER:
      if (flags->permit_pointer)
        return;
      break;
    case DANG_COMPILE_RESULT_LITERAL:
      if (flags->permit_literal)
        return;
      break;
    default:
      dang_assert_not_reached ();
    }

  if (flags->prefer_void)
    {
      dang_compile_result_clear (result_inout, builder);
      dang_compile_result_init_void (result_inout);
      return;
    }

  {
    DangVarId tmp_var;
    DangCompileResult new_res;
    DangCompileResult lvalue, tmp;
    tmp_var = dang_builder_add_tmp (builder, result_inout->any.return_type);
    dang_compile_result_init_stack (&lvalue, result_inout->any.return_type, tmp_var,
                                    FALSE, TRUE, FALSE);
    dang_builder_add_assign (builder, &lvalue, result_inout);
    dang_compile_result_init_stack (&new_res, result_inout->any.return_type, tmp_var,
                                    TRUE,
                                    result_inout->any.is_lvalue,
                                    result_inout->any.is_rvalue);
    dang_compile_result_steal_locks (&new_res, result_inout);
    dang_compile_result_clear (&lvalue, builder);
    tmp = *result_inout;
    *result_inout = new_res;
    dang_compile_result_clear (&tmp, builder);
  }
}

