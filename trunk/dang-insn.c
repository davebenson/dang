#include <string.h>
#include "dang.h"

void
dang_insn_init (DangInsn *insn,
                DangInsnType type)
{
  insn->type = type;
  memset ((char*)insn + sizeof (DangInsnType),
          0, sizeof (DangInsn) - sizeof (DangInsnType));
}

static unsigned
get_param_count_from_sig (DangSignature *sig)
{
  return sig->n_params
      + ((sig->return_type == NULL || sig->return_type == dang_value_type_void ()) ? 0 : 1);
}


void dang_insn_destruct (DangInsn *insn)
{
  unsigned i, np;
  dang_code_position_clear (&insn->base.cp);
  switch (insn->type)
    {
    case DANG_INSN_TYPE_ASSIGN:
      dang_insn_value_clear (&insn->assign.target);
      dang_insn_value_clear (&insn->assign.source);
      break;
    case DANG_INSN_TYPE_CREATE_CLOSURE:
      dang_closure_factory_unref (insn->create_closure.factory);
      if (insn->create_closure.is_literal)
        dang_function_unref (insn->create_closure.underlying.literal);
      dang_free (insn->create_closure.input_vars);
      break;
    case DANG_INSN_TYPE_FUNCTION_CALL:
      np = get_param_count_from_sig (insn->function_call.sig);
      for (i = 0; i < np; i++)
        dang_insn_value_clear (insn->function_call.params + i);
      dang_insn_value_clear (&insn->function_call.function);
      dang_signature_unref (insn->function_call.sig);
      dang_free (insn->function_call.params);
      break;
    case DANG_INSN_TYPE_JUMP_CONDITIONAL:
      dang_insn_value_clear (&insn->jump_conditional.test_value);
      break;
    case DANG_INSN_TYPE_RUN_SIMPLE_C:
      np = get_param_count_from_sig (insn->run_simple_c.func->base.sig);
      for (i = 0; i < np; i++)
        dang_insn_value_clear (insn->run_simple_c.args + i);
      dang_free (insn->run_simple_c.args);
      dang_function_unref (insn->run_simple_c.func);
      break;
    case DANG_INSN_TYPE_INDEX:
      dang_insn_value_clear (&insn->index.container);
      for (i = 0; i < insn->index.index_info->n_indices; i++)
        dang_insn_value_clear (insn->index.indices + i);
      dang_free (insn->index.indices);
      break;
    default:
      break;
    }
}

/* --- DangInsnValue --- */
void
dang_insn_value_from_compile_result (DangInsnValue *out,
                                     DangCompileResult *in)
{
  out->type = in->any.return_type;
  switch (in->type)
    {
    case DANG_COMPILE_RESULT_STACK:
      out->location = DANG_INSN_LOCATION_STACK;
      out->var = in->stack.var_id;
      out->offset = 0;
      out->ns = NULL;
      out->value = NULL;
      break;
    case DANG_COMPILE_RESULT_POINTER:
      out->location = DANG_INSN_LOCATION_POINTER;
      out->var = in->pointer.var_id;
      out->offset = in->pointer.offset;
      out->ns = NULL;
      out->value = NULL;
      break;
    case DANG_COMPILE_RESULT_GLOBAL:
      out->location = DANG_INSN_LOCATION_GLOBAL;
      out->var = DANG_VAR_ID_INVALID;
      out->offset = in->global.ns_offset;
      out->ns = in->global.ns;
      out->value = NULL;
      break;
    case DANG_COMPILE_RESULT_LITERAL:
      out->location = DANG_INSN_LOCATION_LITERAL;
      out->var = DANG_VAR_ID_INVALID;
      out->offset = 0;
      out->ns = NULL;
      out->value = dang_value_copy (out->type, in->literal.value);
      break;
    default:
      dang_assert_not_reached ();
    }
}
void dang_insn_value_clear (DangInsnValue *out)
{
  if (out->location == DANG_INSN_LOCATION_LITERAL && out->value != NULL)
    dang_value_destroy (out->type, out->value);
}

void dang_insn_value_copy  (DangInsnValue *out,
                            DangInsnValue *in)
{
  *out = *in;
  if (out->location == DANG_INSN_LOCATION_LITERAL
      && out->value != NULL)
    out->value = dang_value_copy (out->type, out->value);
}
