
#ifdef DANG_DEBUG
static void
print_psi (DangStringBuffer *buf,
           ParamSourceInfo *psi,
           CompiledSimpleCInvocation *csi)
{
  switch (psi->type)
    {
    case PARAM_SOURCE_TYPE_NULL:
      dang_string_buffer_printf (buf, "null");
      break;
    case PARAM_SOURCE_TYPE_STACK:
      dang_string_buffer_printf (buf, "stack[%u]", psi->info.stack);
      break;
    case PARAM_SOURCE_TYPE_POINTER:
      dang_string_buffer_printf (buf, "stack[%u][%u]",
                                 psi->info.pointer.ptr, psi->info.pointer.offset);
      break;
    case PARAM_SOURCE_TYPE_GLOBAL:
      dang_string_buffer_printf (buf, "global(%s)[%u]",
                                 psi->info.global.ns->full_name, psi->info.global.offset);
      break;
    case PARAM_SOURCE_TYPE_LITERAL:
      {
        char *v = dang_value_to_string (psi->info.literal.type,
                                        (char*)csi + psi->info.literal.offset);
        dang_string_buffer_printf (buf, "literal: %s", v);
        dang_free (v);
      }
      break;
    case PARAM_SOURCE_TYPE_ZERO:
      dang_string_buffer_printf (buf, "zero(%u bytes)", psi->info.zero.size);
      break;
    }
}

static char *
generic_print_run_data (DangStringBuffer *buf,
                        CompiledSimpleCInvocation *csi,
                        size_t *size_out)
{
  ParamSourceInfo *psi = (ParamSourceInfo *) (csi+1);
  unsigned i;
  DangNamespace *ns;
  const char *name;
  if (!dang_debug_query_simple_c (csi->func, &ns, &name))
    dang_string_buffer_printf (buf, "(unregistered) ");
  else
    dang_string_buffer_printf (buf, "(%s:%s) ", ns->full_name, name);
  *size_out = csi->step_size;
  dang_string_buffer_printf (buf, "n_params=%u, tmp_alloc_size=%u: rv=",
                             csi->n_param_source_infos, csi->tmp_alloc);
  print_psi (buf, psi + 0, csi);
  dang_string_buffer_printf (buf, "; params=");
  for (i = 1; i < csi->n_param_source_infos; i++)
    {
      if (i > 1)
        dang_string_buffer_printf (buf, ", ");
      print_psi (buf, psi + i, csi);
    }
  return buf->str;
}

static char *
print_call_simple_c_run_data__heap  (void *step_data,
                                     size_t *size_out)
{
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  dang_string_buffer_printf (&buf, "(on heap) ");
  return generic_print_run_data (&buf, step_data, size_out);
}
static char *
print_call_simple_c_run_data__stack (void *step_data,
                                     size_t *size_out)
{
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  dang_string_buffer_printf (&buf, "(on stack) ");
  return generic_print_run_data (&buf, step_data, size_out);
}

void _dang_function_new_simple_c__register_debug ()
{
  dang_debug_register_run_func (run_simple_c_compiled_heap,
                                "run_simple_c",
                                print_call_simple_c_run_data__heap);
  dang_debug_register_run_func (run_simple_c_compiled_alloca,
                                "run_simple_c",
                                print_call_simple_c_run_data__stack);
}
#endif

#ifdef DANG_DEBUG
#define BEGIN_DEBUG(short_name, structname)  \
static char * \
debug_print__assign_##short_name (void *step_data, \
                              size_t *size_out) \
{ \
  AssignData_##structname *ad = step_data; \
  *size_out = sizeof (AssignData_##structname);
#define END_DEBUG() \
}

BEGIN_DEBUG(memcpy, Memcpy)
  return dang_strdup_printf ("stack[%u] ::= stack[%u] [size=%u]",
                             ad->dst_offset, ad->src_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual, Virtual)
  return dang_strdup_printf ("stack[%u] ::= stack[%u] [type=%s]",
                             ad->dst_offset, ad->src_offset, ad->type->full_name);
END_DEBUG()
BEGIN_DEBUG(memcpy_lptr, Memcpy_Lptr)
  return dang_strdup_printf ("stack[%u][%u] ::= stack[%u] [size=%u]",
                             ad->dst_offset, ad->dst_ptr_offset, ad->src_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_lptr, Virtual_Lptr)
  return dang_strdup_printf ("stack[%u][%u] ::= stack[%u] [type=%s]",
                             ad->dst_offset, ad->dst_ptr_offset, ad->src_offset, ad->type->full_name);
END_DEBUG()
BEGIN_DEBUG(memcpy_lglobal, Memcpy_Lglobal)
  return dang_strdup_printf ("global(%s)[%u] ::= stack[%u] [size=%u]",
                             ad->dst_ns->full_name, ad->dst_offset, ad->src_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_lglobal, Virtual_Lglobal)
  return dang_strdup_printf ("global(%s)[%u] ::= stack[%u] [type=%s]",
                             ad->dst_ns->full_name, ad->dst_offset, ad->src_offset, ad->type->full_name);
END_DEBUG()
BEGIN_DEBUG(memcpy_rptr, Memcpy_Rptr)
  return dang_strdup_printf ("stack[%u] ::= stack[%u][%u] [size=%u]",
                             ad->dst_offset, ad->src_offset, ad->src_ptr_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_rptr, Virtual_Rptr)
  return dang_strdup_printf ("stack[%u] ::= stack[%u][%u] [type=%s]",
                             ad->dst_offset, ad->src_offset, ad->src_ptr_offset, ad->type->full_name);
END_DEBUG()
BEGIN_DEBUG(memcpy_lptr_rptr, Memcpy_Lptr_Rptr)
  return dang_strdup_printf ("stack[%u][%u] ::= stack[%u][%u] [size=%u]",
                             ad->dst_offset, ad->dst_ptr_offset, ad->src_offset, ad->src_ptr_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_lptr_rptr, Virtual_Lptr_Rptr)
  return dang_strdup_printf ("stack[%u][%u] ::= stack[%u][%u] [type=%s]",
                             ad->dst_offset, ad->dst_ptr_offset, ad->src_offset, ad->src_ptr_offset, ad->type->full_name);
END_DEBUG()
BEGIN_DEBUG(memcpy_lglobal_rptr, Memcpy_Lglobal_Rptr)
  return dang_strdup_printf ("global(%s)[%u] ::= stack[%u][%u] [size=%u]",
                             ad->dst_ns->full_name, ad->dst_offset, ad->src_offset, ad->src_ptr_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_lglobal_rptr, Virtual_Lglobal_Rptr)
  return dang_strdup_printf ("global(%s)[%u] ::= stack[%u][%u] [type=%s]",
                             ad->dst_ns->full_name, ad->dst_offset, ad->src_offset, ad->src_ptr_offset, ad->type->full_name);
END_DEBUG()
BEGIN_DEBUG(memcpy_rglobal, Memcpy_Rglobal)
  return dang_strdup_printf ("stack[%u] ::= global(%s)[%u] [size=%u]",
                             ad->dst_offset, ad->src_ns->full_name, ad->src_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_rglobal, Virtual_Rglobal)
  return dang_strdup_printf ("stack[%u] ::= global(%s)[%u] [type=%s]",
                             ad->dst_offset, ad->src_ns->full_name, ad->src_offset, ad->type->full_name);
END_DEBUG()
BEGIN_DEBUG(memcpy_lptr_rglobal, Memcpy_Lptr_Rglobal)
  return dang_strdup_printf ("stack[%u][%u] ::= global(%s)[%u] [size=%u]",
                             ad->dst_offset, ad->dst_ptr_offset, ad->src_ns->full_name, ad->src_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_lptr_rglobal, Virtual_Lptr_Rglobal)
  return dang_strdup_printf ("stack[%u][%u] ::= global(%s)[%u] [type=%s]",
                             ad->dst_offset, ad->dst_ptr_offset, ad->src_ns->full_name, ad->src_offset, ad->type->full_name);
END_DEBUG()
BEGIN_DEBUG(memcpy_lglobal_rglobal, Memcpy_Lglobal_Rglobal)
  return dang_strdup_printf ("global(%s)[%u] ::= global(%s)[%u] [size=%u]",
                             ad->dst_ns->full_name, ad->dst_offset, ad->src_ns->full_name, ad->src_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_lglobal_rglobal, Virtual_Lglobal_Rglobal)
  return dang_strdup_printf ("global(%s)[%u] ::= global(%s)[%u] [type=%s]",
                             ad->dst_ns->full_name, ad->dst_offset, ad->src_ns->full_name, ad->src_offset, ad->type->full_name);
END_DEBUG()

BEGIN_DEBUG(memcpy_rliteral, Memcpy_Rliteral)
  return dang_strdup_printf ("stack[%u] ::= literal [size=%u]",
                             ad->dst_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_rliteral, Virtual_Rliteral)
  return dang_strdup_printf ("stack[%u] ::= literal [type=%s]",
                             ad->dst_offset, ad->type->full_name);
END_DEBUG()
BEGIN_DEBUG(memcpy_lptr_rliteral, Memcpy_Lptr_Rliteral)
  return dang_strdup_printf ("stack[%u][%u] ::= literal [size=%u]",
                             ad->dst_offset, ad->dst_ptr_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_lptr_rliteral, Virtual_Lptr_Rliteral)
  return dang_strdup_printf ("stack[%u][%u] ::= literal [type=%s]",
                             ad->dst_offset, ad->dst_ptr_offset, ad->type->full_name);
END_DEBUG()
BEGIN_DEBUG(memcpy_lglobal_rliteral, Memcpy_Lglobal_Rliteral)
  return dang_strdup_printf ("global(%s)[%u] ::= literal [size=%u]",
                             ad->dst_ns->full_name, ad->dst_offset, ad->size);
END_DEBUG()
BEGIN_DEBUG(virtual_lglobal_rliteral, Virtual_Lglobal_Rliteral)
  return dang_strdup_printf ("global(%s)[%u] ::= literal [type=%s]",
                             ad->dst_ns->full_name, ad->dst_offset, ad->type->full_name);
END_DEBUG()

void _dang_builder_add_assign__register_debug ()
{
#define REGISTER(shortname) \
  dang_debug_register_run_func (assign_##shortname, "assign_" #shortname, \
                                debug_print__assign_##shortname)
  REGISTER (memcpy);
  REGISTER (virtual);
  REGISTER (memcpy_lptr);
  REGISTER (virtual_lptr);
  REGISTER (memcpy_lglobal);
  REGISTER (virtual_lglobal);
  REGISTER (memcpy_rptr);
  REGISTER (virtual_rptr);
  REGISTER (memcpy_lptr_rptr);
  REGISTER (virtual_lptr_rptr);
  REGISTER (memcpy_lglobal_rptr);
  REGISTER (virtual_lglobal_rptr);
  REGISTER (memcpy_rglobal);
  REGISTER (virtual_rglobal);
  REGISTER (memcpy_lptr_rglobal);
  REGISTER (virtual_lptr_rglobal);
  REGISTER (memcpy_lglobal_rglobal);
  REGISTER (virtual_lglobal_rglobal);
  REGISTER (memcpy_rliteral);
  REGISTER (virtual_rliteral);
  REGISTER (memcpy_lptr_rliteral);
  REGISTER (virtual_lptr_rliteral);
  REGISTER (memcpy_lglobal_rliteral);
  REGISTER (virtual_lglobal_rliteral);
}
#endif

#ifdef DANG_DEBUG
static char *
debug_print__run_compiled_function_invocation (void *step_data,
                                               size_t *size_out)
{
  InvocationInputInfo *iii = step_data;
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  unsigned *at = (unsigned *) (iii + 1);
  InvocationInputSubstep *steps = (InvocationInputSubstep*)(at + iii->n_pointers);
  unsigned i;

  if (iii->is_literal_function)
    dang_string_buffer_printf (&buf, "constant function %p; ", iii->func.literal.to_invoke);
  else
    dang_string_buffer_printf (&buf, "function @ stack[%u]; ", iii->func.offset);

  /* print null-pointer exception test pointers */
  if (iii->n_pointers)
    {
      dang_string_buffer_printf (&buf, "null-checks: ");
      for (i = 0; i < iii->n_pointers; i++)
        {
          dang_string_buffer_printf (&buf,
                                     "%sstack[%u]",
                                     i>0 ? "," : "",
                                     at[i]);
        }
      dang_string_buffer_printf (&buf, "; ");
    }

  /* print input steps */
  dang_string_buffer_printf (&buf, "params: ");
  for (i = 0; i < iii->n_steps; i++)
    {
      if (i > 0)
        dang_string_buffer_printf (&buf, ", ");
      switch (steps[i].type)
        {
        case INVOCATION_INPUT_SUBSTEP_ZERO:
          dang_string_buffer_printf (&buf, "zero(new[%u], %u)",
                                     steps[i].called_offset, steps[i].type_info.size);
          break;

        case INVOCATION_INPUT_SUBSTEP_MEMCPY_STACK:
          dang_string_buffer_printf (&buf, "memcpy(new[%u], old[%u], %u)",
                                     steps[i].called_offset,
                                     steps[i].info.stack.src_offset,
                                     steps[i].type_info.size);
          break;
        case INVOCATION_INPUT_SUBSTEP_VIRTUAL_STACK:
          dang_string_buffer_printf (&buf, "virtual(new[%u], old[%u], %s)",
                                     steps[i].called_offset,
                                     steps[i].info.stack.src_offset,
                                     steps[i].type_info.type->full_name);
          break;
        case INVOCATION_INPUT_SUBSTEP_MEMCPY_POINTER:
          dang_string_buffer_printf (&buf, "memcpy(new[%u], old[%u][%u], %u)",
                                     steps[i].called_offset,
                                     steps[i].info.pointer.ptr,
                                     steps[i].info.pointer.offset,
                                     steps[i].type_info.size);
          break;
        case INVOCATION_INPUT_SUBSTEP_VIRTUAL_POINTER:
          dang_string_buffer_printf (&buf, "virtual(new[%u], old[%u][%u], %s)",
                                     steps[i].called_offset,
                                     steps[i].info.pointer.ptr,
                                     steps[i].info.pointer.offset,
                                     steps[i].type_info.type->full_name);
          break;
        case INVOCATION_INPUT_SUBSTEP_MEMCPY_GLOBAL:
          dang_string_buffer_printf (&buf, "memcpy(new[%u], global(%s)[%u], %u)",
                                     steps[i].called_offset,
                                     steps[i].info.global.ns->full_name,
                                     steps[i].info.global.ns_offset,
                                     steps[i].type_info.size);
          break;
        case INVOCATION_INPUT_SUBSTEP_VIRTUAL_GLOBAL:
          dang_string_buffer_printf (&buf, "virtual(new[%u], global(%s)[%u], %s)",
                                     steps[i].called_offset,
                                     steps[i].info.global.ns->full_name,
                                     steps[i].info.global.ns_offset,
                                     steps[i].type_info.type->full_name);
          break;
        case INVOCATION_INPUT_SUBSTEP_MEMCPY_LITERAL:
          dang_string_buffer_printf (&buf, "memcpy(new[%u], $literal, %u)",
                                     steps[i].called_offset,
                                     steps[i].type_info.size);
          break;
        case INVOCATION_INPUT_SUBSTEP_VIRTUAL_LITERAL:
          dang_string_buffer_printf (&buf, "virtual(new[%u], $literal, %s)",
                                     steps[i].called_offset,
                                     steps[i].type_info.type->full_name);
          break;
        }
    }

  *size_out = iii->sizeof_step_data;

  return buf.str;
}
static char *
debug_print__run_compiled_function_output (void *step_data,
                                           size_t *size_out)
{
  InvocationOutputInfo *ioi = step_data;
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  unsigned i;
  InvocationOutputSubstep *steps = (InvocationOutputSubstep*)(ioi+1);
  dang_string_buffer_printf (&buf, "out-params: ");
  for (i = 0; i < ioi->n_steps; i++)
    {
      if (i > 0)
        dang_string_buffer_printf (&buf, ", ");
      switch (steps[i].type)
        {
        case INVOCATION_OUTPUT_SUBSTEP_DESTRUCT:
          dang_string_buffer_printf (&buf, "destruct(new[%u])",
                                     steps[i].called_offset);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_MEMCPY_STACK:
          dang_string_buffer_printf (&buf, "memcpy(old[%u], new[%u], %u)",
                                     steps[i].info.stack.dst_offset,
                                     steps[i].called_offset,
                                     steps[i].type_info.size);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_STACK:
          dang_string_buffer_printf (&buf, "virtual(old[%u], new[%u], %s)",
                                     steps[i].info.stack.dst_offset,
                                     steps[i].called_offset,
                                     steps[i].type_info.type->full_name);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_MEMCPY_POINTER:
          dang_string_buffer_printf (&buf, "memcpy(old[%u][%u], new[%u], %u)",
                                     steps[i].info.pointer.ptr,
                                     steps[i].info.pointer.offset,
                                     steps[i].called_offset,
                                     steps[i].type_info.size);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_POINTER:
          dang_string_buffer_printf (&buf, "virtual(old[%u][%u], new[%u], %s)",
                                     steps[i].info.pointer.ptr,
                                     steps[i].info.pointer.offset,
                                     steps[i].called_offset,
                                     steps[i].type_info.type->full_name);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_MEMCPY_GLOBAL:
          dang_string_buffer_printf (&buf, "memcpy(global(%s)[%u], new[%u], %u)",
                                     steps[i].info.global.ns->full_name,
                                     steps[i].info.global.ns_offset,
                                     steps[i].called_offset,
                                     steps[i].type_info.size);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_GLOBAL:
          dang_string_buffer_printf (&buf, "virtual(global(%s)[%u], new[%u], %s)",
                                     steps[i].info.global.ns->full_name,
                                     steps[i].info.global.ns_offset,
                                     steps[i].called_offset,
                                     steps[i].type_info.type->full_name);
          break;
        }
    }
  *size_out = sizeof (InvocationOutputInfo)
            + ioi->n_steps * sizeof (InvocationOutputSubstep);
  return buf.str;
}

void _dang_compile_function_invocation__register_debug ()
{
  dang_debug_register_run_func (run_compiled_function_invocation,
                                "run_compiled_function_invocation",
                                debug_print__run_compiled_function_invocation);
  dang_debug_register_run_func (run_compiled_function_output,
                                "run_compiled_function_output",
                                debug_print__run_compiled_function_output);
}
#endif
#ifdef DANG_DEBUG
static char *
print_debug__create_closure (void *step_data,
                             unsigned *size_out)
{
  CCCInfo *info = step_data;
  unsigned input_count = dang_closure_factory_get_n_inputs (info->factory);
  *size_out = GET_CCC_INFO_SIZE (input_count);
  return dang_strdup_printf ("function %s; n_inputs=%u",
                             info->is_literal ? "literal" : "on-stack",
                             input_count);
}

void
_dang_debug_init_create_closure(void)
{
  dang_debug_register_run_func (step__create_closure, "create_closure", print_debug__create_closure);
}
#endif

#ifdef DANG_DEBUG
static char *
debug_print__generic (GotoRunDataBase *base,
                      size_t           sizeof_run_data,
                      size_t          *size_out,
                      const char      *format,
                      ...) DANG_GNUC_PRINTF(4,5);
static char *
debug_print__generic_literal (GotoRunDataBase *base,
                      size_t           sizeof_run_data,
                      size_t           *size_out,
                      const char       *main_str)
{
  char *msg;
  *size_out = sizeof_run_data
            + sizeof (GotoDestroy) * base->n_destroy
            + sizeof (GotoInit) * base->n_init;
  msg = dang_strdup_printf ("%s [%u init, %u destroy, %p target]",
                                  main_str,
                                  base->n_init,
                                  base->n_destroy,
                                  base->target);
  return msg;
}
static char *
debug_print__generic (GotoRunDataBase *base,
                      size_t           sizeof_run_data,
                      size_t          *size_out,
                      const char      *format,
                      ...)
{
  va_list args;
  char *str1, *str2;
  va_start (args, format);
  str1 = dang_strdup_vprintf (format, args);
  va_end (args);
  str2 = debug_print__generic_literal (base, sizeof_run_data, size_out, str1);
  dang_free (str1);
  return str2;
}

static char *
debug_print__run_jump         (void *step_data,
                               size_t *size_out)
{
  return debug_print__generic (step_data,
                               sizeof (GotoRunDataBase),
                               size_out, 
                               "goto");
}
static char *
debug_print__run_jump__global (void *step_data,
                               dang_boolean jump_if_zero,
                               size_t *size_out)
{
  GotoRunData_Global *rd = step_data;
  return debug_print__generic (step_data,
                               sizeof (*rd),
                               size_out, 
                               "goto if global %s(%u) %s",
                               rd->ns->full_name,
                               rd->offset,
                               jump_if_zero ? "zero" : "nonzero");
}
static char *
debug_print__run_jump__stack (void *step_data,
                               dang_boolean jump_if_zero,
                               size_t *size_out)
{
  GotoRunData_Stack *rd = step_data;
  return debug_print__generic (step_data,
                               sizeof (*rd),
                               size_out, 
                               "goto if stack[%u] %s",
                               rd->offset,
                               jump_if_zero ? "zero" : "nonzero");
}
static char *
debug_print__run_jump__pointer (void *step_data,
                               dang_boolean jump_if_zero,
                               size_t *size_out)
{
  GotoRunData_Pointer *rd = step_data;
  return debug_print__generic (step_data,
                               sizeof (*rd),
                               size_out, 
                               "goto if stack[%u][%u] %s",
                               rd->ptr, rd->offset,
                               jump_if_zero ? "zero" : "nonzero");
}
static char *
debug_print__run_jump__stack1(void *step_data,
                               dang_boolean jump_if_zero,
                               size_t *size_out)
{
  GotoRunData_Stack1 *rd = step_data;
  return debug_print__generic (step_data,
                               sizeof (*rd),
                               size_out, 
                               "goto if stack[%u] %s (optimized for len=1)",
                               rd->offset,
                               jump_if_zero ? "zero" : "nonzero");
}
static char *
debug_print__run_jump__stack1_simple(void *step_data,
                               dang_boolean jump_if_zero,
                               size_t *size_out)
{
  GotoRunData_Stack1_Simple *rd = step_data;
  *size_out = sizeof (*rd);
  return dang_strdup_printf ("goto %p if stack[%u] %s (optimized for len=1, ndestruct)",
                             rd->target,
                             rd->offset,
                             jump_if_zero ? "zero" : "nonzero");
}
#define DEF_BY_ZERO_AND_NONZERO(tag) \
static char * \
debug_print__run_jump_if_nonzero_##tag (void *step_data, \
                            size_t *size_out) \
{ \
  return debug_print__run_jump__##tag (step_data, FALSE, size_out); \
} \
static char * \
debug_print__run_jump_if_zero_##tag (void *step_data, \
                            size_t *size_out) \
{ \
  return debug_print__run_jump__##tag (step_data, TRUE, size_out); \
}
DEF_BY_ZERO_AND_NONZERO(global)
DEF_BY_ZERO_AND_NONZERO(stack)
DEF_BY_ZERO_AND_NONZERO(pointer)
DEF_BY_ZERO_AND_NONZERO(stack1)
DEF_BY_ZERO_AND_NONZERO(stack1_simple)

static char*
debug_print__run_jump_simple (void *step_data, size_t *size_out)
{
  GotoRunData_Unconditional_Simple *rd = step_data;
  *size_out = sizeof (GotoRunData_Unconditional_Simple);
  return dang_strdup_printf ("goto simple unconditional: %p target",
                             rd->target);
}

void _dang_builder_add_jump__register_debug ()
{
#define REGISTER(shortname) \
  dang_debug_register_run_func (step__jump_##shortname, "step__jump_" #shortname, \
                                debug_print__run_jump_##shortname)
  dang_debug_register_run_func (step__jump, "step__jump", debug_print__run_jump);
  REGISTER(simple);
  REGISTER(if_zero_global);
  REGISTER(if_nonzero_global);
  REGISTER(if_zero_stack);
  REGISTER(if_nonzero_stack);
  REGISTER(if_zero_pointer);
  REGISTER(if_nonzero_pointer);
  REGISTER(if_zero_stack1);
  REGISTER(if_nonzero_stack1);
  REGISTER(if_zero_stack1_simple);
  REGISTER(if_nonzero_stack1_simple);
}
#endif
#ifdef DANG_DEBUG

static char *
debug_print__init_run (void *step_data,
                       size_t *size_out)
{
  InitData *id = step_data;
  *size_out = sizeof (InitData);
  return dang_strdup_printf ("init stack[%u], size %u", id->offset, id->size);
}

void _dang_compile_obey_flags__register_debug ()
{
  dang_debug_register_run_func (init__run,
                                "init_run",
                                debug_print__init_run);
}
#endif
#ifdef DANG_DEBUG
static char *
debug_print__index_stack (void *step_data,
                          size_t *size_out)
{
  IndexStepData *sd = step_data;
  char *rv = dang_strdup_printf ("tensor<%s,%u> at stack[%u]",
                                 sd->element_type->full_name,
                                 sd->rank,
                                 sd->array.stack.array_offset);
  *size_out = sizeof (IndexStepData) + (sd->rank-1) * sizeof (unsigned);
  return rv;
}

static char *
debug_print__index_pointer(void *step_data,
                           size_t *size_out)
{
  IndexStepData *sd = step_data;
  char *rv = dang_strdup_printf ("tensor<%s,%u> at stack[%u][%u]",
                                 sd->element_type->full_name,
                                 sd->rank,
                                 sd->array.pointer.ptr,
                                 sd->array.pointer.offset);
  *size_out = sizeof (IndexStepData) + (sd->rank-1) * sizeof (unsigned);
  return rv;
}
static char *
debug_print__index_global(void *step_data,
                          size_t *size_out)
{
  IndexStepData *sd = step_data;
  char *rv = dang_strdup_printf ("tensor<%s,%u> at ns(%s)[%u]",
                                 sd->element_type->full_name,
                                 sd->rank,
                                 sd->array.global.ns->full_name,
                                 sd->array.global.ns_offset);
  *size_out = sizeof (IndexStepData) + (sd->rank-1) * sizeof (unsigned);
  return rv;
}
#endif


#ifdef DANG_DEBUG
void _dang_debug_init_operator_index ()
{
  dang_debug_register_run_func (step__index_lvalue__stack,
                                "index_lvalue_stack",
                                debug_print__index_stack);
  dang_debug_register_run_func (step__index_rvalue__stack,
                                "index_rvalue_stack",
                                debug_print__index_stack);
  dang_debug_register_run_func (step__index_lvalue__pointer,
                                "index_lvalue_pointer",
                                debug_print__index_pointer);
  dang_debug_register_run_func (step__index_rvalue__pointer,
                                "index_rvalue_pointer",
                                debug_print__index_pointer);
  dang_debug_register_run_func (step__index_lvalue__global,
                                "index_lvalue_global",
                                debug_print__index_global);
  dang_debug_register_run_func (step__index_rvalue__global,
                                "index_rvalue_global",
                                debug_print__index_global);
}
#endif
