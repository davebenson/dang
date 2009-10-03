/* --- ClosureFactory: compiled information about a closure --- */
#include <string.h>
#include "dang.h"
#include "config.h"

typedef enum {
  CLOSURE_PIECE_MEMCPY,
  CLOSURE_PIECE_VIRTUAL
} ClosurePieceType;

typedef struct _ClosurePiece ClosurePiece;
struct _ClosurePiece
{
  ClosurePieceType type;
  unsigned offset;              /* offset in the closure */
  unsigned callee_offset;       /* offset in the called function */
  union {
    DangValueType *virt;
    unsigned size;              /* for memcpy() */
  } info;
};

typedef struct _ZeroRegion ZeroRegion;
struct _ZeroRegion
{
  unsigned offset;
  unsigned size;
};

typedef struct _CopyBackRegion CopyBackRegion;
struct _CopyBackRegion
{
  unsigned offset;              /* offset in the either frame-- they start the same */
  unsigned size;
};

/* Object: DangClosureFactory

   This object makes clusures, which are just DangFunctions
   which call other DangFunctions with certain fixed parameter values.
   
   For example, if you have the addition function:
        function<int a, int b : int>  add
   then you can create a closure like
        f = create_closure(add, 2)
   then f is a function that adds 2 to an integer.
 */
struct _DangClosureFactory
{
  unsigned ref_count;

  /* TODO: OPTIMIZATION: have separate arrays of pieces optimzed for destruct,
     and for contiguous memcpy coagulation */

  /* The arguments copied from dang_function_new_closure() */
  unsigned closure_size;                /* total size in bytes of the DangFunction */
  unsigned n_pieces;
  ClosurePiece *pieces;

  /* The arguments copied from the closure's caller.
   * Because we don't need to do anything with them, we never copy
   * with init_assign; instead we just memcpy() and zero the virtual ones
   * to assist the stack-unwinding.  Even better would be a general
   * StackInfo method to say that one param is aliasing another. */
  unsigned underlying_param_size;       /* copied from caller */
  unsigned n_zero_regions;
  ZeroRegion *zero_regions;

  /* Arguments that must be copied back from the underlying function's frame
   * into our frame.  Basically, any non-memcpy type and any out/inout params. */
  unsigned n_copy_back_regions;
  CopyBackRegion *copy_back_regions;

  DangSignature *result_sig;

  unsigned called_frame_offset;         /* where we put the caller's pointer */

  unsigned closure_frame_size;
};

/*
 * Function: dang_closure_factory_new
 * Create a new closure factory.
 *
 * Parameters:
 *   underlying_sig - signature of the real function that is called
 *   n_params_to_curry - number of parameters to fix (which will no longer 
 *                       be parameters to the returned function.
 *
 * Returns:
 *   the new closure factory.
 */
DangClosureFactory *
dang_closure_factory_new  (DangSignature *underlying_sig,
                           unsigned       n_params_to_curry)
{
  DangClosureFactory *factory = dang_new (DangClosureFactory, 1);
  DangUtilArray copy_back_regions = DANG_UTIL_ARRAY_STATIC_INIT (CopyBackRegion);
  DangUtilArray zero_regions = DANG_UTIL_ARRAY_STATIC_INIT (ZeroRegion);
  unsigned frame_offset;
  unsigned closure_size;
  unsigned i, piece_i;
  dang_assert (underlying_sig->n_params >= n_params_to_curry);
  factory->n_pieces = n_params_to_curry;
  factory->pieces = dang_new (ClosurePiece, n_params_to_curry);
  frame_offset = sizeof(DangThreadStackFrame);
  if (underlying_sig->return_type != NULL
   && underlying_sig->return_type != dang_value_type_void ())
    {
      DangValueType *type = underlying_sig->return_type;
      CopyBackRegion cbr;
      unsigned align = type->alignof_instance;
      unsigned size = type->sizeof_instance;
      frame_offset = DANG_ALIGN (frame_offset, align);
      if (type->destruct)
        {
          ZeroRegion zr = { frame_offset, size };
          dang_util_array_append (&zero_regions, 1, &zr);
        }
      cbr.offset = frame_offset;
      cbr.size = size;
      dang_util_array_append (&copy_back_regions, 1, &cbr);
      frame_offset += size;
    }
  for (i = 0; i < underlying_sig->n_params - n_params_to_curry; i++)
    {
      DangValueType *type = underlying_sig->params[i].type;
      unsigned align = type->alignof_instance;
      unsigned size = type->sizeof_instance;
      frame_offset = DANG_ALIGN (frame_offset, align);
      if (underlying_sig->params[i].type->destruct)
        {
          /* add zero region */
          ZeroRegion zr = { frame_offset, size };
          dang_util_array_append (&zero_regions, 1, &zr);
        }
      if (underlying_sig->params[i].type->destruct
       || underlying_sig->params[i].dir != DANG_FUNCTION_PARAM_IN)
        {
          CopyBackRegion cbr = { frame_offset, size };
          dang_util_array_append (&copy_back_regions, 1, &cbr);
        }
      frame_offset += size;
    }
  factory->underlying_param_size = frame_offset - sizeof (DangThreadStackFrame);

  {
    unsigned closure_frame_offset = frame_offset;
    closure_frame_offset = DANG_ALIGN (closure_frame_offset, DANG_ALIGNOF_POINTER);
    factory->called_frame_offset = closure_frame_offset;
    closure_frame_offset += sizeof (void*);
    factory->closure_frame_size = closure_frame_offset;
  }

  closure_size = sizeof (DangFunction);
  for (piece_i = 0; i < underlying_sig->n_params; i++, piece_i++)
    {
      DangValueType *type = underlying_sig->params[i].type;
      unsigned align = type->alignof_instance;
      unsigned size = type->sizeof_instance;
      closure_size = DANG_ALIGN (closure_size, align);
      frame_offset = DANG_ALIGN (frame_offset, align);
      factory->pieces[piece_i].offset = closure_size;
      factory->pieces[piece_i].callee_offset = frame_offset;
      if (type->init_assign)
        {
          factory->pieces[piece_i].type = CLOSURE_PIECE_VIRTUAL;
          factory->pieces[piece_i].info.virt = type;
        }
      else
        {
          factory->pieces[piece_i].type = CLOSURE_PIECE_MEMCPY;
          factory->pieces[piece_i].info.size = size;
        }
      closure_size += size;
      frame_offset += size;
    }
  factory->closure_size = closure_size;
  factory->n_zero_regions = zero_regions.len;
  factory->zero_regions
    = dang_memdup (zero_regions.data, sizeof(ZeroRegion) * zero_regions.len);
  factory->n_copy_back_regions = copy_back_regions.len;
  factory->copy_back_regions
    = dang_memdup (copy_back_regions.data, sizeof(CopyBackRegion) * copy_back_regions.len);
  dang_util_array_clear (&zero_regions);
  dang_util_array_clear (&copy_back_regions);
  factory->result_sig = dang_signature_new (underlying_sig->return_type,
                                            underlying_sig->n_params - n_params_to_curry,
                                            underlying_sig->params);
  factory->ref_count = 1;
  return factory;
}

/*
 * Function: dang_closure_factory_ref
 * Increase the reference count on the factory.
 *
 * Parameters:
 *    factory - the factory
 *
 * Returns: the factory
 */
DangClosureFactory *dang_closure_factory_ref  (DangClosureFactory*factory)
{
  ++(factory->ref_count);
  return factory;
}

/* Function: dang_closure_factory_unref
 * Reduce the reference-count on the factory, freeing it if it reached 0.
 *
 * Parameters:
 *    factory - the factory
 */
void                dang_closure_factory_unref(DangClosureFactory*factory)
{
  if (--(factory->ref_count) == 0)
    {
      dang_free (factory->pieces);
      dang_free (factory->zero_regions);
      dang_free (factory->copy_back_regions);
      dang_signature_unref (factory->result_sig);
      dang_free (factory);
    }
}

/* Function: dang_closure_factory_get_n_inputs
 * Find the number of curried inputs that this factory accepts.
 *
 * Parameters:
 *     factory - the factory
 *
 * Return value: the number of curried inputs.
 */
unsigned dang_closure_factory_get_n_inputs (DangClosureFactory *factory)
{
  return factory->n_pieces;
}

/* --- Creating the closure --- */

static void
step__closure_invoke  (void                 *step_data,
                       DangThreadStackFrame *stack_frame,
                       DangThread           *thread)
{
  char *frame = (char *) stack_frame;
  unsigned i;
  DangFunctionClosure *closure;
  DangThreadStackFrame **pcalled;
  DangThreadStackFrame *called;
  DangClosureFactory *factory;

  DANG_UNUSED (step_data);

  /* not code to bring home to mother:
     access the closure, because we know its embedded in the function! */
  closure = (DangFunctionClosure *)
         (((char*)(stack_frame->ip)) - offsetof (DangFunctionClosure, steps[0]));
  factory = closure->factory;

  /* The location in the closure's frame where the called functions stack-frame
     will be tucked away. */
  pcalled = (DangThreadStackFrame**)(frame + factory->called_frame_offset);

  /* Allocate the new blank frame and push it on the stack. */
  dang_assert (closure->underlying->type != DANG_FUNCTION_TYPE_STUB);
  dang_assert (closure->underlying->base.frame_size >= sizeof (DangThreadStackFrame));
  called = dang_malloc (closure->underlying->base.frame_size);
  *pcalled = called;
  called->function = closure->underlying;
  called->ip = closure->underlying->base.steps;
  called->caller = stack_frame;
  thread->stack_frame = called;

  /* When the caller returns, go to step__closure_finish */
  dang_thread_stack_frame_advance_ip (stack_frame, 0);

  /* Now, set up variables in called frame */
  memcpy (called + 1, stack_frame + 1, factory->underlying_param_size);
  for (i = 0; i < factory->n_zero_regions; i++)
    memset ((char*)stack_frame + factory->zero_regions[i].offset, 0,
            factory->zero_regions[i].size);
  for (i = 0; i < factory->n_pieces; i++)
    switch (factory->pieces[i].type)
      {
      case CLOSURE_PIECE_MEMCPY:
        memcpy ((char*)called + factory->pieces[i].callee_offset,
                (char*)closure + factory->pieces[i].offset,
                factory->pieces[i].info.size);
        break;
      case CLOSURE_PIECE_VIRTUAL:
        factory->pieces[i].info.virt->init_assign (factory->pieces[i].info.virt,
                                  (char*)called + factory->pieces[i].callee_offset,
                                  (char*)closure + factory->pieces[i].offset);
        break;
      default:
        dang_assert_not_reached ();
      }
}

static void
step__closure_finish  (void                 *step_data,
                       DangThreadStackFrame *stack_frame,
                       DangThread           *thread)
{
  char *frame = (char*)stack_frame;
  DangFunctionClosure *closure;
  DangThreadStackFrame **pcalled;
  DangThreadStackFrame *called;
  DangClosureFactory *factory;
  unsigned i;
  CopyBackRegion *cbr;
  DANG_UNUSED (step_data);
  closure = (DangFunctionClosure *)
         (((char*)(stack_frame->ip)) - offsetof (DangFunctionClosure, steps[1]));
  factory = closure->factory;
  pcalled = (DangThreadStackFrame**)(frame + factory->called_frame_offset);
  called = *pcalled;

  cbr = factory->copy_back_regions;
  for (i = 0; i < factory->n_copy_back_regions; i++)
    memcpy ((char*)stack_frame + cbr[i].offset,
            (char*)called + cbr[i].offset,
            cbr[i].size);
  for (i = 0; i < factory->n_pieces; i++)
    if (factory->pieces[i].type == CLOSURE_PIECE_VIRTUAL)
      factory->pieces[i].info.virt->destruct (factory->pieces[i].info.virt,
                                         (char*)called + factory->pieces[i].callee_offset);
  dang_free (called);


  /* return to caller */
  dang_thread_pop_frame (thread);
}

/* Function: dang_function_new_closure
 * Create a closure which will pass the given values
 * to the underlying function.
 * 
 * Parameters:
 *    factory - the factory for constructing the closure
 *    underlying - the function that the closure should invoke
 *    param_values - the fixed parameters to pass to underlying
 *     (the remaining parameters will be passed to the returned function)
 *
 * Return value: the new function which has the parameter values
 * built into it.
 */
DangFunction *
dang_function_new_closure (DangClosureFactory *factory,
                           DangFunction       *underlying,
                           void              **param_values)
{
  DangFunction *func = dang_malloc (factory->closure_size);
  unsigned i;
  
  func->base.type = DANG_FUNCTION_TYPE_CLOSURE;
  func->base.ref_count = 1;
  func->base.compile = NULL;
  func->base.sig = dang_signature_ref (factory->result_sig);    /* necessary? */
  func->base.frame_size = factory->closure_frame_size;
  func->base.steps = &func->closure.steps[0];
  func->base.is_owned = FALSE;
  func->closure.underlying = dang_function_ref (underlying);
  func->closure.factory = dang_closure_factory_ref (factory);
  func->base.stack_info = dang_new0 (DangFunctionStackInfo, 1);

  func->closure.steps[0].func = step__closure_invoke;
  func->closure.steps[0]._step_data_size = 0;
  func->closure.steps[1].func = step__closure_finish;
  func->closure.steps[1]._step_data_size = 0;
  func->base.stack_info->first_step = func->closure.steps + 0;
  func->base.stack_info->last_step = func->closure.steps + 1;

  for (i = 0; i < factory->n_pieces; i++)
    switch (factory->pieces[i].type)
      {
      case CLOSURE_PIECE_MEMCPY:
        memcpy ((char*)func + factory->pieces[i].offset,
                param_values[i],
                factory->pieces[i].info.size);
        break;
      case CLOSURE_PIECE_VIRTUAL:
        factory->pieces[i].info.virt->init_assign (factory->pieces[i].info.virt,
                                                   (char*)func + factory->pieces[i].offset,
                                                   param_values[i]);
        break;
      default:
        dang_assert_not_reached ();
      }
  return func;
}
void _dang_closure_factory_destruct_closure_data (DangClosureFactory *factory,
                                                  void *function)
{
  unsigned i;
  for (i = 0; i < factory->n_pieces; i++)
    if (factory->pieces[i].type == CLOSURE_PIECE_VIRTUAL)
      {
        DangValueType *type = factory->pieces[i].info.virt;
        void *value = (char*)function + factory->pieces[i].offset;
        type->destruct (type, value);
      }
}
