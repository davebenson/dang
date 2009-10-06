#include <string.h>
#include "dang.h"
#include "config.h"

/* Step to handle the initial step of a general C function. */
static void
run_c_first   (void                 *step_data,
               DangThreadStackFrame *stack_frame,
               DangThread           *thread)
{
  DangFunction *function = stack_frame->function;
  DangSignature *sig = function->base.sig;
  char *frame = (char*) stack_frame;
  void **args = (void**)(frame + function->c.args_frame_offset);
  void *rv;
  unsigned i;
  DANG_UNUSED (step_data);
  DANG_UNUSED (thread);
  if (sig->return_type != NULL)
    rv = frame + function->c.rv_frame_offset;
  else
    rv = NULL;
  for (i = 0; i < sig->n_params; i++)
    args[i] = frame + function->c.arg_frame_offsets[i];
  memset (frame + function->c.state_data_frame_offset, 0, function->c.state_type->sizeof_instance);

  dang_thread_stack_frame_advance_ip (stack_frame, 0);
}


static void
run_c_nonfirst   (void                 *step_data,
                  DangThreadStackFrame *stack_frame,
                  DangThread           *thread)
{
  DangFunction *function = stack_frame->function;
  char *frame = (char *) stack_frame;
  void **args = (void **) (frame + function->c.args_frame_offset);
  void *rv = frame + function->c.rv_frame_offset;
  DangError *error = NULL;
  dang_assert (function->type == DANG_FUNCTION_TYPE_C);
  DANG_UNUSED (step_data);
  switch (function->c.func (thread, args, rv,
                            frame + function->c.state_data_frame_offset,
                            function->c.func_data, &error))
    {
    case DANG_C_FUNCTION_ERROR:
      dang_thread_throw (thread, dang_value_type_error (), &error);
      dang_error_unref (error);
      return;
    case DANG_C_FUNCTION_SUCCESS:
      thread->stack_frame = stack_frame->caller;
      {
        DangValueType *type = function->c.state_type;
        if (type && type->destruct != NULL)
          type->destruct (type, (char*)stack_frame + function->c.state_data_frame_offset);
      }
      break;
    case DANG_C_FUNCTION_BEGAN_CALL:
      dang_assert (thread->stack_frame->caller == stack_frame);
      return;
    case DANG_C_FUNCTION_YIELDED:
      thread->status = DANG_THREAD_STATUS_YIELDED;
      thread->info.yield.yield_cancel_func = NULL;
      thread->info.yield.yield_cancel_func_data = NULL;
      thread->info.yield.done_func = NULL;
      thread->info.yield.done_func_data = NULL;
      return;
    }
}


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
  DangFunctionStackInfo *stack_info;
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
  rv->c.state_type = state_type;
  rv->c.func = func;
  rv->c.func_data = func_data;
  rv->c.func_data_destroy = func_data_destroy;

  /* Compute argument packing info */
  offset = sizeof (DangThreadStackFrame);
  if (sig->return_type != NULL)
    {
      offset = DANG_ALIGN (offset, sig->return_type->alignof_instance);
      rv->c.rv_frame_offset = offset;
      offset += sig->return_type->sizeof_instance;
    }
  rv->c.arg_frame_offsets = dang_new (unsigned, sig->n_params);
  for (i = 0; i < sig->n_params; i++)
    {
      offset = DANG_ALIGN (offset, sig->params[i].type->alignof_instance);
      rv->c.arg_frame_offsets[i] = offset;
      offset += sig->params[i].type->sizeof_instance;
    }
  offset = DANG_ALIGN (offset, state_type->alignof_instance);
  rv->c.state_data_frame_offset = offset;
  offset += state_type->sizeof_instance;
  offset = DANG_ALIGN (offset, DANG_ALIGNOF_POINTER);
  rv->c.args_frame_offset = offset;
  offset += sizeof (void*) * sig->n_params;
  rv->c.subcall_frame_offset = offset;
  offset += sizeof (void*);
  rv->base.frame_size = offset;

  stack_info = dang_new0 (DangFunctionStackInfo, 1);
  rv->base.stack_info = stack_info;
  stack_info->first_step = rv->base.steps + 0;
  stack_info->last_step = rv->base.steps + 1;
  stack_info->vars = dang_new (DangFunctionStackVarInfo, 1);
  /* XXX: variable liveness non-inclusive... this is weird */
  stack_info->vars[0].start = rv->base.steps + 0;
  stack_info->vars[0].end = rv->base.steps + 2;
  stack_info->vars[0].offset = rv->c.state_data_frame_offset;
  stack_info->vars[0].type = state_type;

  return rv;
}

/* NOTE: only input and inout arguments must be given. */
DangCFunctionResult
dang_c_function_begin_subcall (DangThread *thread,
                               DangFunction *function,
                               void        **args)
{
  DangSignature *sig = function->base.sig;
  unsigned offset = sizeof (DangThreadStackFrame);
  DangThreadStackFrame *new_frame = dang_malloc (function->base.frame_size);
  unsigned i;
  DangFunction *caller = thread->stack_frame->function;
  dang_assert (caller->type == DANG_FUNCTION_TYPE_C);
  if (sig->return_type)
    {
      DangValueType *type = sig->return_type;
      offset = DANG_ALIGN (offset, type->alignof_instance);
      memset ((char*)new_frame + offset, 0, type->sizeof_instance);
      offset += type->sizeof_instance;
    }
  for (i = 0; i < sig->n_params; i++)
    {
      DangValueType *type = sig->params[i].type;
      offset = DANG_ALIGN (offset, type->alignof_instance);
      if (sig->params[i].dir == DANG_FUNCTION_PARAM_IN
       || sig->params[i].dir == DANG_FUNCTION_PARAM_INOUT)
        {
          if (type->init_assign)
            type->init_assign (type, (char*)new_frame + offset, args[i]);
          else
            memcpy ((char*)new_frame + offset, args[i], type->sizeof_instance);
        }
      else
        memset ((char*) new_frame + offset, 0, type->sizeof_instance);
      offset += sig->params[i].type->sizeof_instance;
    }

  * (void**)((char*)thread->stack_frame + caller->c.subcall_frame_offset) = new_frame;
  new_frame->caller = thread->stack_frame;
  new_frame->function = function;
  new_frame->ip = function->base.steps;
  thread->stack_frame = new_frame;

  return DANG_C_FUNCTION_BEGAN_CALL;
}

/* NOTE: only inout and output arguments must be given */
void                dang_c_function_end_subcall   (DangThread *thread,
                                                   DangFunction *function,
                                                   void        **args,
                                                   void         *rv_out)
{
  unsigned offset = sizeof (DangThreadStackFrame);
  DangSignature *sig = function->base.sig;
  DangFunction *caller = thread->stack_frame->function;
  DangThreadStackFrame *old_frame = * (void**)((char*)thread->stack_frame + caller->c.subcall_frame_offset);
  unsigned i;
  dang_assert (caller->type == DANG_FUNCTION_TYPE_C);
  dang_assert (old_frame->function == function);
  dang_assert (old_frame->caller == thread->stack_frame);

  if (sig->return_type)
    {
      DangValueType *type = sig->return_type;
      offset = DANG_ALIGN (offset, type->alignof_instance);
      if (type->init_assign)                    /* XXX: assign() or init_assign() ??? */
        type->init_assign (type, rv_out, (char*)old_frame + offset);
      else
        memcpy (rv_out, (char*)old_frame + offset, type->sizeof_instance);
      offset += type->sizeof_instance;
    }
  for (i = 0; i < sig->n_params; i++)
    {
      DangValueType *type = sig->params[i].type;
      offset = DANG_ALIGN (offset, type->alignof_instance);
      if (sig->params[i].dir == DANG_FUNCTION_PARAM_OUT
       || sig->params[i].dir == DANG_FUNCTION_PARAM_INOUT)
        {
          if (type->destruct != NULL)
            type->destruct (type, args[i]);
          memcpy (args[i], (char*)old_frame + offset, type->sizeof_instance);
        }
      else if (type->destruct)
        {
          type->destruct (type, (char*)old_frame + offset);
        }
      offset += sig->params[i].type->sizeof_instance;
    }
  dang_free (old_frame);
}
