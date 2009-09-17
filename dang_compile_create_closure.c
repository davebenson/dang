#include "dang.h"

void
dang_compile_create_closure(DangBuilder     *builder,
                            DangCodePosition        *cp,
                            DangCompileResult       *func,
                            unsigned                 n_vars,
                            DangVarId               *vars,
                            DangCompileResult       *result)
{
  DangSignature *sig;
  DangInsn insn;
  dang_assert (dang_value_type_is_function (func->any.return_type));

  sig = ((DangValueTypeFunction*)func->any.return_type)->sig;
  if (sig->n_params < n_vars)
    {
      dang_compile_result_set_error (result, cp,
                                     "dang_compile_create_closure: underlying func has too few params (%u v %u)", sig->n_params, n_vars);
      return;
    }
  if (n_vars == 0)
    {
      if (func->type == DANG_COMPILE_RESULT_LITERAL)
        {
          DangFunction *fct;
          dang_compile_result_init_literal (result,
                                            func->any.return_type,
                                            func->literal.value);
          fct = * (DangFunction**) func->literal.value;
          if (dang_function_needs_registration (fct))
            dang_compile_context_register (builder->function->stub.cc, fct);
        }
      else
        {
          /* HACK: need dang_compile_result_init_copy() or something */
          *result = *func;
        }
      return;
    }
  DangSignature *new_sig;
  DangValueType *rv_type;
  DangVarId var;
  new_sig = dang_signature_new (sig->return_type,
                                sig->n_params - n_vars,
                                sig->params);
  rv_type = dang_value_type_function (new_sig);
  dang_signature_unref (new_sig);

  dang_insn_init (&insn, DANG_INSN_TYPE_CREATE_CLOSURE);
  switch (func->type)
    {
    case DANG_COMPILE_RESULT_LITERAL:
      insn.create_closure.is_literal = TRUE;
      insn.create_closure.underlying.literal = *(DangFunction**)func->literal.value;
      dang_function_ref (insn.create_closure.underlying.literal);
      if (dang_function_needs_registration (insn.create_closure.underlying.literal))
        dang_compile_context_register (builder->function->stub.cc, insn.create_closure.underlying.literal);
      break;
    case DANG_COMPILE_RESULT_STACK:
      insn.create_closure.is_literal = FALSE;
      insn.create_closure.underlying.function_var = func->stack.var_id;
      break;
    default:
      /* copy to stack */
      {
        DangVarId tmp = dang_builder_add_tmp (builder, func->any.return_type);
        DangCompileResult lvalue;
        dang_compile_result_init_stack (&lvalue, func->any.return_type, tmp,
                                        FALSE, TRUE, FALSE);
        dang_builder_add_assign (builder, &lvalue, func);
        dang_compile_result_clear (&lvalue, builder);

        insn.create_closure.is_literal = FALSE;
        insn.create_closure.underlying.function_var = tmp;
      }
      break;
    }
  var = dang_builder_add_tmp (builder, rv_type);
  dang_builder_note_var_create (builder, var);
  insn.create_closure.target = var;
  insn.create_closure.factory = dang_closure_factory_new (sig, n_vars);
  insn.create_closure.input_vars = dang_memdup (vars, n_vars * sizeof (DangVarId));
  dang_builder_add_insn (builder, &insn);
  dang_compile_result_init_stack (result, rv_type, var,
                                  TRUE, FALSE, TRUE);
}

