#include "dang.h"

static void
append_var (DangVarId var,
            DangBuilderVariable *vars,
            DangStringBuffer *out)
{
  if (vars[var].name)
    dang_string_buffer_printf (out, "%s ($%u)", vars[var].name, var);
  else
    dang_string_buffer_printf (out, "$%u", var);
  dang_string_buffer_printf (out, " [%s]", vars[var].type->full_name);
}

static void
append_location (DangInsnValue *value,
                 DangBuilderVariable *vars,
                 DangStringBuffer *out)
{
  switch (value->location)
    {
    case DANG_INSN_LOCATION_STACK:
      append_var (value->var, vars, out);
      break;
    case DANG_INSN_LOCATION_POINTER:
      if (vars[value->var].name)
        dang_string_buffer_printf (out, "%s ($%u)[%u]", vars[value->var].name, value->var, value->offset);
      else
        dang_string_buffer_printf (out, "$%u[%u]", value->var, value->offset);
      dang_string_buffer_printf (out, " [%s]", value->type->full_name);
      break;
    case DANG_INSN_LOCATION_GLOBAL:
      dang_string_buffer_printf (out,"%s[%u] [%s]",
                                 value->ns->full_name, value->offset,
                                 value->type->full_name);
      break;
    case DANG_INSN_LOCATION_LITERAL:
      {
        char *str = dang_value_to_string (value->type, value->value);
        dang_string_buffer_printf (out, "%s:",value->type->full_name);
        dang_string_buffer_append (out, str);
        dang_free (str);
        break;
      }
    }
}

static void
append_label (DangLabelId label,
              DangBuilderLabel *labels,
              DangStringBuffer *out)
{
  DangBuilderLabel *lab = labels + label;
  if (lab->type == DANG_FUNCTION_BUILDER_LABEL_TYPE_NAMED)
    dang_string_buffer_printf (out, "%s", lab->name);
  else
    dang_string_buffer_printf (out, "LABEL$%u", label);
}

void
dang_insn_dump (DangInsn *insn,
                DangBuilderVariable *vars,
                DangBuilderLabel *labels,
                DangStringBuffer *out)
{
  switch (insn->type)
    {
    case DANG_INSN_TYPE_INIT:
      {
        dang_string_buffer_append (out, "    INIT ");
        append_var (insn->init.var, vars, out);
        dang_string_buffer_append (out, "\n");
        break;
      }
    case DANG_INSN_TYPE_DESTRUCT:
      {
        dang_string_buffer_append (out, "    DESTRUCT ");
        append_var (insn->destruct.var, vars, out);
        dang_string_buffer_append (out, "\n");
        break;
      }
    case DANG_INSN_TYPE_ASSIGN:
      {
        dang_string_buffer_append (out, "    ASSIGN ");
        append_location (&insn->assign.target, vars, out);
        dang_string_buffer_append (out, ", ");
        append_location (&insn->assign.source, vars, out);
        dang_string_buffer_append (out, "\n");
        break;
      }
    case DANG_INSN_TYPE_JUMP:
      {
        dang_string_buffer_append (out, "    JUMP ");
        append_label (insn->jump.target, labels, out);
        dang_string_buffer_append (out, "\n");
        break;
      }
    case DANG_INSN_TYPE_JUMP_CONDITIONAL:
      {
        dang_string_buffer_printf (out, "    JUMP %s ",
                                   insn->jump_conditional.jump_if_zero ? "IF_ZERO" : "IF_NONZERO");
        append_location (&insn->jump_conditional.test_value, vars, out);
        dang_string_buffer_append (out, " ");
        append_label (insn->jump.target, labels, out);
        dang_string_buffer_append (out, "\n");
        break;
      }
    case DANG_INSN_TYPE_FUNCTION_CALL:
      {
        unsigned i;
        DangSignature *sig = insn->function_call.sig;
        unsigned rv_offset = (sig->return_type == NULL
                           || sig->return_type == dang_value_type_void()) ? 0 : 1;
        dang_string_buffer_printf (out, "    CALL ");
        append_location (&insn->function_call.function, vars, out);
        dang_string_buffer_append (out, "(");
        for (i = 0; i < insn->function_call.sig->n_params; i++)
          {
            if (i > 0)
              dang_string_buffer_append (out, ", ");
            if (insn->function_call.sig->params[i].dir != DANG_FUNCTION_PARAM_IN)
              dang_string_buffer_append (out, "& ");
            append_location (insn->function_call.params + i + rv_offset,
                             vars, out);
          }
        dang_string_buffer_append (out, ")");
        if (rv_offset)
          {
            dang_string_buffer_append (out, " -> ");
            append_location (insn->function_call.params, vars, out);
          }
        dang_string_buffer_append (out, "\n");
        break;
      }
    case DANG_INSN_TYPE_RUN_SIMPLE_C:
      {
        unsigned i;
        DangSignature *sig = insn->run_simple_c.func->base.sig;
        unsigned rv_offset = (sig->return_type == NULL
                           || sig->return_type == dang_value_type_void()) ? 0 : 1;
        dang_string_buffer_printf (out, "    CALL ");
        {
          char *str = dang_function_to_string (insn->run_simple_c.func);
          dang_string_buffer_append (out, str);
          dang_free (str);
        }
        dang_string_buffer_append (out, "(");
        for (i = 0; i < sig->n_params; i++)
          {
            if (i > 0)
              dang_string_buffer_append (out, ", ");
            if (sig->params[i].dir != DANG_FUNCTION_PARAM_IN)
              dang_string_buffer_append (out, "& ");
            append_location (insn->run_simple_c.args + i + rv_offset,
                             vars, out);
          }
        dang_string_buffer_append (out, ")");
        if (rv_offset)
          {
            dang_string_buffer_append (out, " -> ");
            append_location (insn->run_simple_c.args, vars, out);
          }
        dang_string_buffer_append (out, "\n");
        break;
      }
    case DANG_INSN_TYPE_PUSH_CATCH_GUARD:
      dang_string_buffer_printf (out, "    PUSH_CATCH_GUARD %u\n",
                                 insn->push_catch_guard.catch_block_index);
      break;
    case DANG_INSN_TYPE_POP_CATCH_GUARD:
      dang_string_buffer_append (out, "    POP_CATCH_GUARD\n");
      break;
    case DANG_INSN_TYPE_RETURN:
      dang_string_buffer_append (out, "    RETURN\n");
      break;
    case DANG_INSN_TYPE_INDEX:
      {
        DangValueIndexInfo *ii = insn->index.index_info;
        unsigned i;
        dang_string_buffer_append (out, "    INDEX ");
        append_location (&insn->index.container, vars, out);
        dang_string_buffer_append (out, " [");
        for (i = 0; i < ii->n_indices; i++)
          {
            if (i > 0)
              dang_string_buffer_append (out, ", ");
            append_location (&insn->index.indices[i], vars, out);
          }
        dang_string_buffer_append (out, "]");
        if (insn->index.is_set)
          dang_string_buffer_append (out, " <- ");
        else
          dang_string_buffer_append (out, " -> ");
        append_location (&insn->index.element, vars, out);
        dang_string_buffer_append (out, "\n");
      }
      break;
    case DANG_INSN_TYPE_CREATE_CLOSURE:
      {
        unsigned n_vars = dang_closure_factory_get_n_inputs (insn->create_closure.factory);
        unsigned i;
        dang_string_buffer_append (out, "    CREATE_CLOSURE ");
        append_var (insn->create_closure.target, vars, out);
        dang_warning ("insn: create_closure: is_literal=%u",insn->create_closure.is_literal);
        if (insn->create_closure.is_literal)
          {
            char *str = dang_function_to_string (insn->create_closure.underlying.literal);
            dang_string_buffer_append (out, str);
            dang_free (str);
          }
        else
          append_var (insn->create_closure.underlying.function_var, vars, out);
        dang_string_buffer_append (out, " : ");
        for (i = 0; i < n_vars; i++)
          {
            if (i > 0)
              dang_string_buffer_append (out, ", ");
            append_var (insn->create_closure.input_vars[i], vars, out);
          }
        dang_string_buffer_append (out, "\n");
      }
      break;
    }
}
