#include <string.h>
#include <stdio.h>
#include "dang.h"

const char *dang_thread_status_name (DangThreadStatus status)
{
  switch (status)
    {
    case DANG_THREAD_STATUS_NOT_STARTED:  return "not_started";
    case DANG_THREAD_STATUS_RUNNING:      return "running";
    case DANG_THREAD_STATUS_YIELDED:      return "yielded";
    case DANG_THREAD_STATUS_DONE:         return "done";
    case DANG_THREAD_STATUS_THREW:        return "threw";
    case DANG_THREAD_STATUS_CANCELLED:    return "cancelled";
    }
  return "*bad-status*";
}

void dang_thread_pop_frame (DangThread *thread)
{
  DangThreadStackFrame *stack_frame = thread->stack_frame;
  thread->stack_frame = stack_frame->caller;
  if (thread->stack_frame == NULL)
    {
      thread->status = DANG_THREAD_STATUS_DONE;
      dang_thread_unref (thread);
    }
}

static DangThreadStackFrame *
dang_thread_push_frame (DangThread   *thread,
                        DangFunction *function,
                        void        **arguments)
{
  DangSignature *sig = function->base.sig;
  DangThreadStackFrame *frame = dang_malloc (function->base.frame_size);
  unsigned offset = sizeof (DangThreadStackFrame);
  unsigned i;
  frame->function = function;
  if (sig->return_type && sig->return_type != dang_value_type_void ())
    {
      DangValueType *type = sig->return_type;
      offset = DANG_ALIGN (offset, type->alignof_instance);
      memset ((char*)frame + offset, 0, type->sizeof_instance);
      offset += type->sizeof_instance;
    }
  for (i = 0; i < sig->n_params; i++)
    {
      DangValueType *type = sig->params[i].type;
      unsigned align = type->alignof_instance;
      unsigned size = type->sizeof_instance;
      offset = DANG_ALIGN (offset, align);
      if (sig->params[i].dir == DANG_FUNCTION_PARAM_OUT)
        {
          /* zero */
          memset ((char*)frame + offset, 0, size);
        }
      else
        {
          /* init-assign / memcpy */
          if (type->init_assign)
            type->init_assign (type, (char*)frame + offset, arguments[i]);
          else
            memcpy ((char*)frame + offset, arguments[i], size);
        }
      offset += size;
    }
  frame->ip = function->base.steps;
  frame->caller = thread->stack_frame;
  thread->stack_frame = frame;
  return frame;
}

void
dang_thread_pop_catch_guard (DangThread *thread)
{
  DangThreadCatchGuard *kill = thread->catch_guards;
  thread->catch_guards = thread->catch_guards->parent;
  dang_free (kill);
}

void
dang_thread_push_catch_guard (DangThread *thread,
                              DangThreadStackFrame *stack_frame,
                              DangCatchBlock *catch_block)
{
  DangThreadCatchGuard *catch;
  DangFunctionStackInfo *stack_info = thread->stack_frame->function->base.stack_info;
  dang_assert (stack_info->catch_blocks <= catch_block
            && catch_block < stack_info->catch_blocks + stack_info->n_catch_blocks);
  catch = dang_new (DangThreadCatchGuard, 1);
  catch->parent = thread->catch_guards;
  catch->stack_frame = stack_frame;
  catch->catch_block = catch_block;
  thread->catch_guards = catch;
}

DangThread   *
dang_thread_new (DangFunction *function,
                 unsigned      n_arguments,
                 void        **arguments)
{
  DangThread *thread = dang_new (DangThread, 1);
  dang_assert (function->base.sig->n_params == n_arguments);
  thread->status = DANG_THREAD_STATUS_NOT_STARTED;
  thread->stack_frame = NULL;
  thread->ref_count = 1;
  thread->catch_guards = NULL;
  thread->rv_frame = dang_thread_push_frame (thread, function, arguments);
  thread->rv_function = function;
  dang_thread_ref (thread);   /* unref'd when the last frame is popped */
  return thread;
}

/* Return TRUE if the thread must be unrefed */
static dang_boolean
dang_thread_unwind_one_frame (DangThread *thread)
{
  DangThreadStackFrame *kill;
  DangFunctionStackInfo *stack_info;
  unsigned i;

  dang_assert (thread->stack_frame != NULL);

  kill = thread->stack_frame;

  /* Unwind catch blocks added by this stack-frame */
  while (thread->catch_guards != NULL
      && thread->catch_guards->stack_frame == kill)
    dang_thread_pop_catch_guard (thread);

  /* Destruct variables that have destructors */
  stack_info = kill->function->base.stack_info;
  for (i = 0; i < stack_info->n_vars; i++)
    {
      if (stack_info->vars[i].start < kill->ip
       && kill->ip < stack_info->vars[i].end
       && stack_info->vars[i].type->destruct)
        {
          unsigned offset = stack_info->vars[i].offset;
          DangValueType *type = stack_info->vars[i].type;
          void *value = ((char*)kill) + offset;
          type->destruct (type, value);
        }
    }

  /* XXX: someday, permit input parameters to be memcpyd */
  for (i = 0; i < stack_info->n_params; i++)
    {
      if (stack_info->params[i].type->destruct)
        {
          unsigned offset = stack_info->params[i].offset;
          DangValueType *type = stack_info->params[i].type;
          void *value = ((char*)kill) + offset;
          type->destruct (type, value);
        }
    }


  kill = thread->stack_frame;
  thread->stack_frame = kill->caller;
  dang_free (kill);

  return (thread->stack_frame == NULL);
}

/* dynamically go to the catch block destination.
 * (may initialize, destruct vars,
 * create or destroy catch_guards)
 */
/* implement the goto to the catch block.
 * (may initialize, destruct vars,
 * create or destroy catch_guards)
 */
static void
dang_thread_force_goto (DangThread *thread,
                        DangStep   *new_ip)
{
  DangThreadStackFrame *frame = thread->stack_frame;
  DangFunction *function = frame->function;
  DangFunctionStackInfo *stack_info = function->base.stack_info;
  DangStep *old_ip = thread->stack_frame->ip;
  DangCatchBlock **new_catch_blocks;
  unsigned n_new_catch_blocks = 0;
  DangCatchBlock **old_catch_blocks;
  unsigned n_old_catch_blocks = 0;
  unsigned max_common, n_common;
  DangThreadCatchGuard *catch;
  unsigned i;
  dang_assert (stack_info != NULL);
  dang_assert (stack_info->first_step <= new_ip
               && new_ip <= stack_info->last_step);

  /* Compute the new catch blocks that should be active at the target label. */
  new_catch_blocks = dang_new (DangCatchBlock *, stack_info->n_catch_blocks);
  for (i = 0; i < stack_info->n_catch_blocks; i++)
    {
      DangCatchBlock *block = stack_info->catch_blocks + i;
      if (block->start < new_ip && new_ip <= block->end)
        new_catch_blocks[n_new_catch_blocks++] = block;
      else if (new_ip > block->end)
        break;                  /* since the list is sorted, we can bail out */
    }

  /* Compute the old catch blocks that should be active */
  for (catch = thread->catch_guards;
       catch != NULL && catch->stack_frame == thread->stack_frame;
       catch = catch->parent)
    n_old_catch_blocks++;
  catch = thread->catch_guards;
  old_catch_blocks = dang_new (DangCatchBlock *, n_old_catch_blocks);
  for (i = 0; i < n_old_catch_blocks; i++)
    {
      old_catch_blocks[n_old_catch_blocks - 1 - i] = catch->catch_block;
      catch = catch->parent;
    }

  /* Find how many catch blocks are the same for both the source and target ip */
  max_common = DANG_MIN (n_new_catch_blocks, n_old_catch_blocks);
  for (n_common = 0; n_common < max_common; n_common++)
    if (new_catch_blocks[n_common] != old_catch_blocks[n_common])
      break;

  /* Pop old catch blocks */
  for (i = 0; i < n_old_catch_blocks - n_common; i++)
    dang_thread_pop_catch_guard (thread);

  /* Push new catch blocks */
  for (i = n_common; i < n_new_catch_blocks; i++)
    dang_thread_push_catch_guard (thread, frame, new_catch_blocks[i]);

  dang_free (old_catch_blocks);
  dang_free (new_catch_blocks);


  /* Compute dead and live variables */
  for (i = 0; i < stack_info->n_vars; i++)
    {
      DangFunctionStackVarInfo *v = stack_info->vars + i;
      dang_boolean was_live = (v->start < old_ip && old_ip <= v->end);
      dang_boolean will_be_live = (v->start < new_ip && new_ip <= v->end);
      if (was_live && !will_be_live)
        {
          if (v->type->destruct != NULL)
            {
              v->type->destruct (v->type, (char*)frame + v->offset);
            }
        }
      else if (!was_live && will_be_live)
        {
          memset ((char*)frame + v->offset, 0, v->type->sizeof_instance);
        }
    }

  /* finally, jump to the new address */
  frame->ip = new_ip;
}

static void
resume_running (DangThread *thread)
{
resume_running:

  while (DANG_LIKELY (thread->status == DANG_THREAD_STATUS_RUNNING))
    {
      DangStep *step = thread->stack_frame->ip;
#ifdef DANG_DEBUG
#endif
      step->func (step + 1, thread->stack_frame, thread);
    }
  switch (thread->status)
    {
    case DANG_THREAD_STATUS_NOT_STARTED:
    case DANG_THREAD_STATUS_RUNNING:
      dang_assert_not_reached ();
      break;
    case DANG_THREAD_STATUS_THREW:
      {
        /* see if we can find a catch() block that matches the type thrown */
        DangThreadStackFrame *unwind_dest;
        DangCatchBlockClause *clause;
        while (thread->catch_guards)
          {
            DangThreadCatchGuard *kill;
            if (dang_catch_block_is_applicable (thread->catch_guards->catch_block,
                                                thread->info.threw.type,
                                                &clause))
              break;
            kill = thread->catch_guards;
            thread->catch_guards = thread->catch_guards->parent;
            dang_free (kill);
          }
        unwind_dest = thread->catch_guards ? thread->catch_guards->stack_frame : NULL;

        /* unwind the stack to that point. */
        dang_assert (thread->stack_frame != NULL);
        while (thread->stack_frame != unwind_dest)
          dang_thread_unwind_one_frame (thread);

        /* if uncaught, return. */
        if (thread->catch_guards == NULL)
          {
            dang_thread_unref (thread);
            return;
          }

        /* jump, fixing up variables and exception blocks;
           this will free the catch block we are currently in */
        dang_thread_force_goto (thread, clause->catch_target);

        if (clause->catch_var_offset != 0)
          {
            memcpy ((char*)unwind_dest + clause->catch_var_offset,
                    thread->info.threw.value,
                    thread->info.threw.type->sizeof_instance);
            dang_free (thread->info.threw.value);
          }
        else if (thread->info.threw.value)
          {
            if (thread->info.threw.type->destruct)
              thread->info.threw.type->destruct (thread->info.threw.type,
                                                 thread->info.threw.value);
            dang_free (thread->info.threw.value);
          }

        thread->status = DANG_THREAD_STATUS_RUNNING;
        goto resume_running;
      }
    case DANG_THREAD_STATUS_YIELDED:
    case DANG_THREAD_STATUS_DONE:
    case DANG_THREAD_STATUS_CANCELLED:
      break;
    }
}


void
dang_thread_run (DangThread   *thread)
{
  if (thread->status == DANG_THREAD_STATUS_RUNNING)
    {
      dang_warning ("dang_thread_run() called while already running");
      return;
    }
  if (thread->status != DANG_THREAD_STATUS_NOT_STARTED)
    {
      dang_warning ("dang_thread_run() called thread was in an invalid state '%s'",
                    dang_thread_status_name (thread->status));
      return;
    }
  thread->status = DANG_THREAD_STATUS_RUNNING;
  resume_running (thread);
}

void
dang_thread_resume (DangThread *thread)
{
  dang_assert (thread->status == DANG_THREAD_STATUS_YIELDED);
  thread->status = DANG_THREAD_STATUS_RUNNING;
  resume_running (thread);
}

void
dang_thread_cancel(DangThread   *thread)
{
  dang_boolean must_unref = FALSE;
  if (thread->status == DANG_THREAD_STATUS_CANCELLED
   || thread->status == DANG_THREAD_STATUS_DONE)
    return;

  if (thread->status == DANG_THREAD_STATUS_YIELDED)
    {
      /* cancel yield callback */
      thread->info.yield.yield_cancel_func (thread->info.yield.yield_cancel_func_data);
    }
  if (thread->stack_frame != NULL)
    {
      while (thread->stack_frame)
        dang_thread_unwind_one_frame (thread);
      must_unref = TRUE;
    }
  thread->status = DANG_THREAD_STATUS_CANCELLED;
  if (must_unref)
    dang_thread_unref (thread);
}

void
dang_thread_throw(DangThread   *thread,
                  DangValueType *type,
                  void          *value)
{
  dang_assert (thread->status == DANG_THREAD_STATUS_RUNNING);
  if (type == dang_value_type_error ())
    {
      DangError *e = *(DangError**)value;
      if (e->backtrace == NULL)
        e->backtrace = dang_thread_get_backtrace (thread);
    }
  thread->status = DANG_THREAD_STATUS_THREW;
  thread->info.threw.type = type;
  thread->info.threw.value = dang_value_copy (type, value);
}

void
dang_thread_throw_error (DangThread *thread,
                         DangError  *error)
{
  dang_thread_throw (thread, dang_value_type_error (), &error);
}

void
dang_thread_throw_null_pointer_exception (DangThread *thread)
{
  DangError *error = dang_error_new ("null pointer exception");
  dang_thread_throw_error (thread, error);
  dang_error_unref (error);
}

void
dang_thread_throw_array_bounds_exception (DangThread *thread)
{
  DangError *error = dang_error_new ("out-of-bounds array access");
  dang_thread_throw_error (thread, error);
  dang_error_unref (error);
}

char *
dang_thread_get_backtrace (DangThread *thread)
{
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  DangThreadStackFrame *frame;
  for (frame = thread->stack_frame; frame; frame = frame->caller)
    {
      DangCodePosition cp;
      if (dang_function_get_code_position (frame->function, frame->ip, &cp))
        {
          dang_string_buffer_printf (&buf, "  %s:%u\n", cp.filename->str, cp.line);
          dang_code_position_clear (&cp);
        }
      else
        dang_string_buffer_printf (&buf, "  (unknown position: %p)\n", frame->ip);
    }
  return buf.str;
}

DangThread *
dang_thread_ref (DangThread *thread)
{
  ++(thread->ref_count);
  return thread;
}

void
dang_thread_unref (DangThread *thread)
{
  if (--(thread->ref_count) == 0)
    {
      dang_assert (thread->stack_frame == NULL);
      if (thread->rv_frame != NULL
       && thread->status == DANG_THREAD_STATUS_DONE)
        {
          DangSignature *sig = thread->rv_function->base.sig;
          unsigned offset = sizeof (DangThreadStackFrame);
          unsigned i;
          if (sig->return_type != NULL)
            {
              offset = DANG_ALIGN (offset, sig->return_type->alignof_instance);
              if (sig->return_type->destruct)
                sig->return_type->destruct (sig->return_type,
                                        (char*)thread->rv_frame + offset);
              offset += sig->return_type->sizeof_instance;
            }
          for (i = 0; i < sig->n_params; i++)
            {
              DangValueType *type = sig->params[i].type;
              offset = DANG_ALIGN (offset, type->alignof_instance);
              if (type->destruct)
                type->destruct (type, (char*)thread->rv_frame + offset);
              offset += type->sizeof_instance;
            }
          dang_free (thread->rv_frame);
        }
      else if (thread->status == DANG_THREAD_STATUS_THREW)
        {
          if (thread->info.threw.type)
            {
              DangValueType *type = thread->info.threw.type;
              if (type->destruct)
                type->destruct (type, thread->info.threw.value);
              dang_free (thread->info.threw.value);
            }
        }
      dang_free (thread);
    }
}

void dang_thread_stack_frame_advance_ip(DangThreadStackFrame *frame,
                            size_t      sizeof_step_data)
{
  DangStep *ip = frame->ip;
  dang_assert (ip->_step_data_size == sizeof_step_data);
  frame->ip = (DangStep *) ((char*)(ip+1) + sizeof_step_data);
}
