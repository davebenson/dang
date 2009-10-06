#include <string.h>
#include "dang.h"

/* XXX: move into dang_util -- this is duped in simple_c */
static inline unsigned
align_offset (DangValueType *type,
               unsigned       offset)
{
  offset += (type->alignof_instance - 1);
  offset &= ~(type->alignof_instance - 1);
  return offset;
}

static inline unsigned
adjust_offset (DangValueType *type,
               unsigned       offset)
{
  offset += (type->alignof_instance - 1);
  offset &= ~(type->alignof_instance - 1);
  offset += type->sizeof_instance;
  return offset;
}

/* Step to handle the initial step of a general C function. */
static void
run_c_first   (void                 *step_data,
               DangThreadStackFrame *stack_frame,
               DangThread           *thread)
{
  DangFunction *function = stack_frame->function;
  DangSignature *sig = function->base.sig;
  void **args;
  void *rv;
  unsigned offset = sizeof (DangThreadStackFrame);
  unsigned i;
  DangError *error = NULL;
  DANG_UNUSED (step_data);
  DANG_UNUSED (thread);
  if (sig->return_type != NULL)
    {
      offset = align_offset (sig->return_type, offset);
      rv = (char*)stack_frame + offset;
      offset += sig->return_type->sizeof_instance;
    }
  else
    rv = NULL;
  args = alloca (sizeof (void*) * sig->n_params);
  for (i = 0; i < sig->n_params; i++)
    {
      offset = align_offset (sig->params[i].type, offset);
      args[i] = (char*)stack_frame + offset;
      offset += sig->params[i].type->sizeof_instance;
    }

  dang_thread_stack_frame_advance_ip (frame, 0);
}


static void
run_c_nonfirst   (void                 *step_data,
                  DangThreadStackFrame *stack_frame,
                  DangThread           *thread)
{
  DangFunction *function = stack_frame->function;
  void **args = (void **) ((char*)stack_frame + function->args_frame_offset);
  switch (function->c.func (thread, args, rv,
                            (char*)stack_frame + function->state_data_offset,
                            function->c.func_data, &error))
    {
    case DANG_C_FUNCTION_ERROR:
      dang_thread_throw (thread, dang_value_type_error (), &error);
      dang_error_unref (error);
      return;
    case DANG_C_FUNCTION_SUCCESS:
      thread->stack_frame = stack_frame->caller;
      break;
    case DANG_C_FUNCTION_BEGAN_CALL:
      dang_assert (thread->stack_frame->caller == stack_frame);
      return;
    case DANG_C_FUNCTION_YIELDED:
      thread->status = DANG_THREAD_STATUS_YIELDED;
      thread->yield_cancel_func = NULL;
      thread->yield_cancel_func_data = NULL;
      thread->done_func = NULL;
      thread->done_func_data = NULL;
      return;
    }
}

#if 0
static void
compile_c (DangFunction        *function,
           DangBuilder *builder,
           DangCompileResult   *return_value_info,
           unsigned             n_params,
           DangCompileResult   *params)
{
  DangInsn insn;
  unsigned n_args = (return_value_info ? 1 : 0) + n_params;
  unsigned out = 0;
  unsigned i;

  if (return_value_info)
    {
      dang_compile_result_force_initialize (builder, return_value_info);
      dang_assert (function->base.sig->return_type != NULL
                   && function->base.sig->return_type != dang_value_type_void ());
    }
  for (i = 0; i < n_params; i++)
    dang_compile_result_force_initialize (builder, params + i);

  dang_insn_init (&insn, DANG_INSN_TYPE_RUN_SIMPLE_C);
  insn.run_simple_c.func = dang_function_ref (function);
  insn.run_simple_c.args = dang_new (DangInsnValue, n_args);
  if (return_value_info)
    dang_insn_value_from_compile_result (insn.run_simple_c.args + out++,
                                         return_value_info);
  for (i = 0; i < n_params; i++)
    dang_insn_value_from_compile_result (insn.run_simple_c.args + out++,
                                         params + i);
  dang_builder_add_insn (builder, &insn);
}
#endif

DangFunction *
dang_function_new_c        (DangSignature   *sig,
                            DangValueType   *state_type,
                            DangCFunc        func,
                            void            *func_data,
                            DangDestroyNotify func_data_destroy)
{
  DangFunction *rv;
  unsigned offset;
  unsigned i;
  rv = dang_new (DangFunction, 1);
  rv->base.type = DANG_FUNCTION_TYPE_C;
  rv->base.ref_count = 1;
  rv->base.compile = NULL;
  rv->base.stack_info = NULL;
  rv->base.sig = dang_signature_ref (sig);
  rv->base.steps = dang_new (DangStep, 2);
  rv->base.steps[0].func = run_c_first;
  rv->base.steps[0]._step_data_size = 0;
  rv->base.steps[1].func = run_c_nonfirst;
  rv->base.steps[1]._step_data_size = 0;
  rv->base.is_owned = FALSE;

  /* Compute argument packing info */
  offset = sizeof (DangThreadStackFrame);
  if (sig->return_type != NULL)
    offset = adjust_offset (sig->return_type, offset);
  for (i = 0; i < sig->n_params; i++)
    offset = adjust_offset (sig->params[i].type, offset);
  rv->base.frame_size = offset;

  rv->c.state_type = state_type;
  rv->c.func = func;
  rv->c.func_data = func_data;
  rv->c.func_data_destroy = func_data_destroy;
  return rv;
}

