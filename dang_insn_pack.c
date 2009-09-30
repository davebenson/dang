#include <string.h>
#include "config.h"
#include "dang.h"

typedef struct _PackedValue PackedValue;
struct _PackedValue
{
  DangInsnLocation location;    /* for all values */
  union {
    unsigned stack;             /* offset in bytes */
    struct {
      unsigned stack_offset, ptr_offset;
    } pointer;
    struct {
      DangNamespace *ns;
      unsigned ns_offset;
    } global;
    struct {
      void *value;
    } literal;
  } info;
};

/* if this returns NULL, you must throw a null-pointer exception */
static void *
peek_packed_value (PackedValue *pv,
                   DangThreadStackFrame *stack_frame)
{
  char *frame = (char*) stack_frame;
  switch (pv->location)
    {
    case DANG_INSN_LOCATION_STACK:
      return frame + pv->info.stack;
    case DANG_INSN_LOCATION_POINTER:
      {
        void *ptr = * (void **) (frame + pv->info.pointer.stack_offset);
        if (ptr == NULL)
          {
            return NULL;
          }
        return (char*)ptr + pv->info.pointer.ptr_offset;
      }
    case DANG_INSN_LOCATION_GLOBAL:
      return pv->info.global.ns->global_data + pv->info.global.ns_offset;
    case DANG_INSN_LOCATION_LITERAL:
      return pv->info.literal.value;
    }
  dang_assert_not_reached ();
  return NULL;
}

static void
pack_value_location (DangInsnPackContext *context,
                     DangInsnValue       *in,
                     PackedValue         *out)
{
  out->location = in->location;
  switch (in->location)
    {
    case DANG_INSN_LOCATION_STACK:
      out->info.stack = context->vars[in->var].offset;
      break;
    case DANG_INSN_LOCATION_POINTER:
      out->info.pointer.stack_offset = context->vars[in->var].offset;
      out->info.pointer.ptr_offset = in->offset;
      break;
    case DANG_INSN_LOCATION_GLOBAL:
      out->info.global.ns = in->ns;
      out->info.global.ns_offset = in->offset;
      break;
    case DANG_INSN_LOCATION_LITERAL:
      out->info.literal.value = in->value;
      in->value = NULL;
      break;
    }
}

/* --- helper functions --- */
static void
dang_insn_pack_context_append (DangInsnPackContext *context,
                               DangStepRun          func,
                               unsigned             step_data_size,
                               const void          *step_data,
                               DangDestroyNotify    step_data_destroy)
{
  DangStep step = { func, step_data_size };
  unsigned step_data_offset = context->step_data.len + sizeof (DangStep);
                      
  dang_util_array_append (&context->step_data, sizeof (step), &step);
  dang_util_array_append (&context->step_data, step_data_size, step_data);
  if (step_data_destroy != NULL)
    {
      DangInsnDestroy des = { TRUE,
                              (DangInsnDestroyNotify) step_data_destroy,
                              step_data_offset,
                              NULL,         /* no 'first_arg': it'll be the step */
                              NULL };       /* no 'second_arg': for step_data_destroy */
      dang_util_array_append (&context->destroys, 1, &des);
    }
}
static void
dang_insn_pack_context_add_destroy (DangInsnPackContext *context,
                                    DangInsnDestroyNotify destroy,
                                    void *arg1,
                                    void *arg2)
{
  DangInsnDestroy des = { FALSE, destroy, 0, arg1, arg2 };
  dang_util_array_append (&context->destroys, 1, &des);
}

static void
dang_insn_pack_context_note_target (DangInsnPackContext *context,
                                    DangLabelId          target,
                                    unsigned             offset_in_next_step_data)
{
  DangInsnLabelFixup fixup;
  fixup.step_data_offset = context->step_data.len
                         + sizeof (DangStep)
                         + offset_in_next_step_data;
  fixup.label = target;
  dang_util_array_append (&context->label_fixups, 1, &fixup);
}


/* === INIT === */
typedef struct _InitData InitData;
struct _InitData
{
  unsigned offset;                      /* initiallize the var-id */
  unsigned size;
};

static void
step__init (void                 *step_data,
            DangThreadStackFrame *stack_frame,
            DangThread           *thread)
{
  InitData *id = step_data;
  DANG_UNUSED (thread);
  memset ((char*)stack_frame + id->offset, 0, id->size);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (InitData));
}

static void
pack__init (DangInsn *insn,
            DangInsnPackContext *context)
{
  InitData id = { context->vars[insn->init.var].offset,
                  context->vars[insn->init.var].type->sizeof_instance };
  dang_insn_pack_context_append (context, step__init, sizeof (id), &id, NULL);
}

/* === DESTRUCT === */
typedef struct _DestructInfo DestructInfo;
struct _DestructInfo
{
  DangValueType *type;
  unsigned offset;
};
static void
step__destruct (void                 *step_data,
                DangThreadStackFrame *stack_frame,
                DangThread           *thread)
{
  DestructInfo *di = step_data;
  DANG_UNUSED (thread);
  di->type->destruct (di->type, (char*) stack_frame + di->offset);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof(DestructInfo));
}

static void
pack__destruct (DangInsn *insn,
                DangInsnPackContext *context)
{
  DestructInfo di;
  di.type = context->vars[insn->destruct.var].type;
  di.offset = context->vars[insn->destruct.var].offset;
  dang_insn_pack_context_append (context, step__destruct, sizeof (DestructInfo), &di, NULL);
}

/* === ASSIGN === */
/* tedious, multicased implementation of dang_builder_add_assign().
 * we make a fairly brutalizing effort at efficiency here.
 * Is it worth it? Probably not, but assignments ARE incredibly
 * common.  Often they work their way in to the implementations
 * of other functions.
 */
/* variants:
 *    memcpy   -- memcpy (needs size)
 *    virtual  -- type-wise virtual (needs two pointers)
 *    lptr     -- lvalue is pointer inside lvalue that's on stack
 *    rptr     -- rvalue is pointer inside rvalue that's on stack
 *    lglobal  -- lvalue is global
 *    rglobal  -- rvalue is global
 *    rliteral -- rvalue is constant (aka a literal or a value)
 * 
 * All needed variants satify the constraints:
 *   memcpy == !virtual
 *   (lptr + lglobal <= 1)              ... ie at most one may be set
 *   (rptr + rglobal + rliteral <= 1)   ... ie at most one may be set
 */
typedef struct _AssignPatchData AssignPatchData;
struct _AssignPatchData
{
  /* An array of offsets into the step_data (the various
     AssignData structures below) giving variable-ids
     initially, but which must be mapped to
     variable stack-offsets before execution. */
  unsigned n_var_id_locations;
  unsigned var_id_locations[2];
};

typedef struct {
  unsigned dst_offset, src_offset, size;
} AssignData_Memcpy;
static void
assign_memcpy   (void                 *step_data,
                 DangThreadStackFrame *stack_frame,
                 DangThread           *thread)
{
  AssignData_Memcpy *ad = step_data;
  DANG_UNUSED (thread);
  memcpy (((char*)stack_frame) + ad->dst_offset,
          ((char*)stack_frame) + ad->src_offset,
          ad->size);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (AssignData_Memcpy));
}

typedef struct {
  unsigned dst_offset, src_offset;
  DangValueType *type;
  DangValueAssignFunc func;
} AssignData_Virtual;
static void
assign_virtual  (void                 *step_data,
                 DangThreadStackFrame *stack_frame,
                 DangThread           *thread)
{
  AssignData_Virtual *ad = step_data;
  DANG_UNUSED (thread);
  ad->func (ad->type,
            ((char*)stack_frame) + ad->dst_offset,
            ((char*)stack_frame) + ad->src_offset);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (AssignData_Virtual));
}

static void
throw_null_pointer_exception (DangThread *thread)
{
  DangError *error = dang_error_new ("null-pointer exception");
  dang_thread_throw (thread, dang_value_type_error (), &error);
  dang_error_unref (error);
}

typedef struct {
  unsigned dst_offset, dst_ptr_offset, src_offset, size;
} AssignData_Memcpy_Lptr;

static void
assign_memcpy_lptr  (void                 *step_data,
                     DangThreadStackFrame *stack_frame,
                     DangThread           *thread)
{
  AssignData_Memcpy_Lptr *ad = step_data;
  void *lhs0 = * (void **) ((char*) stack_frame + ad->dst_offset);
  if (DANG_UNLIKELY (lhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      memcpy (((char*)lhs0) + ad->dst_ptr_offset,
              ((char*)stack_frame) + ad->src_offset,
              ad->size);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}

typedef struct {
  unsigned dst_offset, dst_ptr_offset, src_offset;
  DangValueType *type;
} AssignData_Virtual_Lptr;

static void
assign_virtual_lptr (void                 *step_data,
                     DangThreadStackFrame *stack_frame,
                     DangThread           *thread)
{
  AssignData_Virtual_Lptr *ad = step_data;
  void *lhs0 = * (void **) ((char*) stack_frame + ad->dst_offset);
  if (DANG_UNLIKELY (lhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      ad->type->assign (ad->type,
                        ((char*)lhs0) + ad->dst_ptr_offset,
                        ((char*)stack_frame) + ad->src_offset);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}

typedef struct {
  unsigned dst_offset, src_offset, src_ptr_offset, size;
} AssignData_Memcpy_Rptr;

static void
assign_memcpy_rptr  (void                 *step_data,
                     DangThreadStackFrame *stack_frame,
                     DangThread           *thread)
{
  AssignData_Memcpy_Rptr *ad = step_data;
  void *rhs0 = * (void **) ((char*) stack_frame + ad->src_offset);
  if (DANG_UNLIKELY (rhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      memcpy (((char*)stack_frame) + ad->dst_offset,
              ((char*) rhs0) + ad->src_ptr_offset,
              ad->size);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}

typedef struct {
  unsigned dst_offset, src_offset, src_ptr_offset;
  DangValueType *type;
  DangValueAssignFunc func;
} AssignData_Virtual_Rptr;

static void
assign_virtual_rptr  (void                 *step_data,
                     DangThreadStackFrame *stack_frame,
                     DangThread           *thread)
{
  AssignData_Virtual_Rptr *ad = step_data;
  void *rhs0 = * (void **) ((char*) stack_frame + ad->src_offset);
  if (DANG_UNLIKELY (rhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      ad->func (ad->type,
                ((char*)stack_frame) + ad->dst_offset,
                ((char*) rhs0) + ad->src_ptr_offset);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}

typedef struct {
  unsigned dst_offset, dst_ptr_offset, src_offset, src_ptr_offset, size;
} AssignData_Memcpy_Lptr_Rptr;

static void
assign_memcpy_lptr_rptr (void                 *step_data,
                         DangThreadStackFrame *stack_frame,
                         DangThread           *thread)
{
  AssignData_Memcpy_Lptr_Rptr *ad = step_data;
  void *lhs0 = * (void **) ((char*) stack_frame + ad->dst_offset);
  void *rhs0 = * (void **) ((char*) stack_frame + ad->src_offset);
  if (DANG_UNLIKELY (lhs0 == NULL)
   || DANG_UNLIKELY (rhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      memcpy (((char*)lhs0) + ad->dst_ptr_offset,
              ((char*) rhs0) + ad->src_ptr_offset,
              ad->size);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}

typedef struct {
  unsigned dst_offset, dst_ptr_offset, src_offset, src_ptr_offset;
  DangValueType *type;
} AssignData_Virtual_Lptr_Rptr;

static void
assign_virtual_lptr_rptr (void                 *step_data,
                          DangThreadStackFrame *stack_frame,
                          DangThread           *thread)
{
  AssignData_Virtual_Lptr_Rptr *ad = step_data;
  void *lhs0 = * (void **) ((char*) stack_frame + ad->dst_offset);
  void *rhs0 = * (void **) ((char*) stack_frame + ad->src_offset);
  if (DANG_UNLIKELY (lhs0 == NULL)
   || DANG_UNLIKELY (rhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      ad->type->assign (ad->type,
                        ((char*)lhs0) + ad->dst_ptr_offset,
                        ((char*) rhs0) + ad->src_ptr_offset);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}

typedef struct {
  DangNamespace *dst_ns;
  unsigned dst_offset, src_offset, size;
} AssignData_Memcpy_Lglobal;

static void
assign_memcpy_lglobal  (void                 *step_data,
                        DangThreadStackFrame *stack_frame,
                        DangThread           *thread)
{
  AssignData_Memcpy_Lglobal *ad = step_data;
  DANG_UNUSED (thread);
  memcpy (ad->dst_ns->global_data + ad->dst_offset,
          ((char*)stack_frame) + ad->src_offset,
          ad->size);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
}

typedef struct {
  DangNamespace *dst_ns;
  unsigned dst_offset, src_offset;
  DangValueType *type;
} AssignData_Virtual_Lglobal;

static void
assign_virtual_lglobal (void                 *step_data,
                     DangThreadStackFrame *stack_frame,
                     DangThread           *thread)
{
  AssignData_Virtual_Lglobal *ad = step_data;
  DANG_UNUSED (thread);
  ad->type->assign (ad->type,
                    ad->dst_ns->global_data + ad->dst_offset,
                    ((char*)stack_frame) + ad->src_offset);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
}

typedef struct {
  DangNamespace *dst_ns;
  unsigned dst_offset, src_offset, src_ptr_offset, size;
} AssignData_Memcpy_Lglobal_Rptr;

static void
assign_memcpy_lglobal_rptr (void                 *step_data,
                            DangThreadStackFrame *stack_frame,
                            DangThread           *thread)
{
  AssignData_Memcpy_Lglobal_Rptr *ad = step_data;
  void *rhs0 = * (void **) ((char*) stack_frame + ad->src_offset);
  if (DANG_UNLIKELY (rhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      memcpy (ad->dst_ns->global_data + ad->dst_offset,
              ((char*) rhs0) + ad->src_ptr_offset,
              ad->size);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}

typedef struct {
  DangNamespace *dst_ns;
  unsigned dst_offset, src_offset, src_ptr_offset;
  DangValueType *type;
} AssignData_Virtual_Lglobal_Rptr;

static void
assign_virtual_lglobal_rptr (void                 *step_data,
                             DangThreadStackFrame *stack_frame,
                             DangThread           *thread)
{
  AssignData_Virtual_Lglobal_Rptr *ad = step_data;
  void *rhs0 = * (void **) ((char*) stack_frame + ad->src_offset);
  if (DANG_UNLIKELY (rhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      ad->type->assign (ad->type,
                        ad->dst_ns->global_data + ad->dst_offset,
                        ((char*) rhs0) + ad->src_ptr_offset);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}

typedef struct {
  DangNamespace *src_ns;
  unsigned dst_offset, src_offset, size;
} AssignData_Memcpy_Rglobal;

static void
assign_memcpy_rglobal  (void                 *step_data,
                        DangThreadStackFrame *stack_frame,
                        DangThread           *thread)
{
  AssignData_Memcpy_Rglobal *ad = step_data;
  DANG_UNUSED (thread);
  memcpy (((char*)stack_frame) + ad->dst_offset,
          ad->src_ns->global_data + ad->src_offset,
          ad->size);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
}

typedef struct {
  DangNamespace *src_ns;
  unsigned dst_offset, src_offset;
  DangValueType *type;
  DangValueAssignFunc func;
} AssignData_Virtual_Rglobal;

static void
assign_virtual_rglobal  (void                 *step_data,
                     DangThreadStackFrame *stack_frame,
                     DangThread           *thread)
{
  AssignData_Virtual_Rglobal *ad = step_data;
  DANG_UNUSED (thread);
  ad->func (ad->type,
            ((char*)stack_frame) + ad->dst_offset,
            ad->src_ns->global_data + ad->src_offset);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
}

typedef struct {
  DangNamespace *src_ns;
  unsigned dst_offset, dst_ptr_offset, src_offset, size;
} AssignData_Memcpy_Lptr_Rglobal;

static void
assign_memcpy_lptr_rglobal (void                 *step_data,
                            DangThreadStackFrame *stack_frame,
                            DangThread           *thread)
{
  AssignData_Memcpy_Lptr_Rglobal *ad = step_data;
  void *lhs0 = * (void **) ((char*) stack_frame + ad->dst_offset);
  if (DANG_UNLIKELY (lhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      memcpy (((char*)lhs0) + ad->dst_ptr_offset,
              ad->src_ns->global_data + ad->src_offset,
              ad->size);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}

typedef struct {
  DangNamespace *src_ns;
  unsigned dst_offset, dst_ptr_offset, src_offset;
  DangValueType *type;
} AssignData_Virtual_Lptr_Rglobal;

static void
assign_virtual_lptr_rglobal (void                 *step_data,
                             DangThreadStackFrame *stack_frame,
                             DangThread           *thread)
{
  AssignData_Virtual_Lptr_Rglobal *ad = step_data;
  void *lhs0 = * (void **) ((char*) stack_frame + ad->dst_offset);
  if (DANG_UNLIKELY (lhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      ad->type->assign (ad->type,
                        ((char*)lhs0) + ad->dst_ptr_offset,
                        ad->src_ns->global_data + ad->src_offset);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}


typedef struct {
  DangNamespace *dst_ns, *src_ns;
  unsigned dst_offset, src_offset, size;
} AssignData_Memcpy_Lglobal_Rglobal;

static void
assign_memcpy_lglobal_rglobal (void                 *step_data,
                               DangThreadStackFrame *stack_frame,
                               DangThread           *thread)
{
  AssignData_Memcpy_Lglobal_Rglobal *ad = step_data;
  DANG_UNUSED (stack_frame);
  DANG_UNUSED (thread);
  memcpy (ad->dst_ns->global_data + ad->dst_offset,
          ad->src_ns->global_data + ad->src_offset,
          ad->size);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
}

typedef struct {
  DangNamespace *dst_ns, *src_ns;
  unsigned dst_offset, src_offset;
  DangValueType *type;
} AssignData_Virtual_Lglobal_Rglobal;

static void
assign_virtual_lglobal_rglobal (void                 *step_data,
                                DangThreadStackFrame *stack_frame,
                                DangThread           *thread)
{
  AssignData_Virtual_Lglobal_Rglobal *ad = step_data;
  DANG_UNUSED (stack_frame);
  DANG_UNUSED (thread);
  ad->type->assign (ad->type,
                    ad->dst_ns->global_data + ad->dst_offset,
                    ad->src_ns->global_data + ad->src_offset);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
}


typedef struct {
  unsigned dst_offset, size;
  void *src_value;
} AssignData_Memcpy_Rliteral;

static void
assign_memcpy_rliteral  (void                 *step_data,
                        DangThreadStackFrame *stack_frame,
                        DangThread           *thread)
{
  AssignData_Memcpy_Rliteral *ad = step_data;
  DANG_UNUSED (thread);
  memcpy (((char*)stack_frame) + ad->dst_offset,
          ad->src_value,
          ad->size);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
}

typedef struct {
  unsigned dst_offset;
  DangValueType *type;
  DangValueAssignFunc func;
  void *src_value;
} AssignData_Virtual_Rliteral;

static void
assign_virtual_rliteral  (void                 *step_data,
                     DangThreadStackFrame *stack_frame,
                     DangThread           *thread)
{
  AssignData_Virtual_Rliteral *ad = step_data;
  DANG_UNUSED (thread);
  ad->func (ad->type,
            ((char*)stack_frame) + ad->dst_offset,
            ad->src_value);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
}

typedef struct {
  unsigned dst_offset, dst_ptr_offset, size;
  void *src_value;
} AssignData_Memcpy_Lptr_Rliteral;

static void
assign_memcpy_lptr_rliteral (void                 *step_data,
                            DangThreadStackFrame *stack_frame,
                            DangThread           *thread)
{
  AssignData_Memcpy_Lptr_Rliteral *ad = step_data;
  void *lhs0 = * (void **) ((char*) stack_frame + ad->dst_offset);
  if (DANG_UNLIKELY (lhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      memcpy (((char*)lhs0) + ad->dst_ptr_offset,
              ad->src_value,
              ad->size);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}

typedef struct {
  unsigned dst_offset, dst_ptr_offset;
  void *src_value;
  DangValueType *type;
} AssignData_Virtual_Lptr_Rliteral;

static void
assign_virtual_lptr_rliteral (void                 *step_data,
                             DangThreadStackFrame *stack_frame,
                             DangThread           *thread)
{
  AssignData_Virtual_Lptr_Rliteral *ad = step_data;
  void *lhs0 = * (void **) ((char*) stack_frame + ad->dst_offset);
  if (DANG_UNLIKELY (lhs0 == NULL))
    {
      throw_null_pointer_exception (thread);
    }
  else
    {
      ad->type->assign (ad->type,
                        ((char*)lhs0) + ad->dst_ptr_offset,
                        ad->src_value);
      dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
    }
}


typedef struct {
  DangNamespace *dst_ns;
  unsigned dst_offset, src_offset, size;
  void *src_value;
} AssignData_Memcpy_Lglobal_Rliteral;

static void
assign_memcpy_lglobal_rliteral (void                 *step_data,
                               DangThreadStackFrame *stack_frame,
                               DangThread           *thread)
{
  AssignData_Memcpy_Lglobal_Rliteral *ad = step_data;
  DANG_UNUSED (stack_frame);
  DANG_UNUSED (thread);
  memcpy (ad->dst_ns->global_data + ad->dst_offset,
          ad->src_value,
          ad->size);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
}

typedef struct {
  DangNamespace *dst_ns;
  unsigned dst_offset;
  void *src_value;
  DangValueType *type;
} AssignData_Virtual_Lglobal_Rliteral;

static void
assign_virtual_lglobal_rliteral (void                 *step_data,
                                DangThreadStackFrame *stack_frame,
                                DangThread           *thread)
{
  AssignData_Virtual_Lglobal_Rliteral *ad = step_data;
  DANG_UNUSED (stack_frame);
  DANG_UNUSED (thread);
  ad->type->assign (ad->type,
                    ad->dst_ns->global_data + ad->dst_offset,
                    ad->src_value);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*ad));
}

///static dang_boolean
///do_assign_patching  (DangBuilder *builder,
///                     DangIntermediateStep *step_inout,
///                     DangError           **error)
///{
///  AssignPatchData *patch_data = step_inout->patch_data;
///  unsigned i;
///  DANG_UNUSED (error);
///  dang_assert (patch_data->n_var_id_locations == 1
///            || patch_data->n_var_id_locations == 2);
///  for (i = 0; i < patch_data->n_var_id_locations; i++)
///    {
///      unsigned *off;
///      DangBuilderVariable *vars = builder->vars.data;
///      off = (unsigned *) ((char*)step_inout->step_data + patch_data->var_id_locations[i]);
///      dang_assert (*off < builder->vars.len);
///      *off = vars[*off].offset;
///    }
///  return TRUE;
///}
///
static void
pack__assign (DangInsn *insn,
              DangInsnPackContext *context)
{
  DangInsnValue *lvalue = &insn->assign.target;
  DangInsnValue *rvalue = &insn->assign.source;
  dang_boolean use_memcpy = insn->assign.target.type->init_assign == NULL;
  DangValueType *type = lvalue->type;
#define LRVALUE(l,r)   (((l)<<4) | (r))
#define LRVALUE_S(l,r)   LRVALUE(DANG_INSN_LOCATION_##l, DANG_INSN_LOCATION_##r)
#define DECLARE_AND_INIT_PROTOTYPE(Suffix, suffix) \
          AssignData_##Suffix prototype
#define HANDLE_LVALUE_STACK() \
          prototype.dst_offset = context->vars[lvalue->var].offset;
#define HANDLE_LVALUE_POINTER() \
          prototype.dst_offset = context->vars[lvalue->var].offset; \
          prototype.dst_ptr_offset = lvalue->offset
#define HANDLE_LVALUE_GLOBAL() \
          prototype.dst_ns = lvalue->ns; \
          prototype.dst_offset = lvalue->offset
#define HANDLE_VIRTUAL() \
          prototype.type = type
#define HANDLE_VIRTUAL__LVALUE_STACK() \
          prototype.type = type; \
          do {if (insn->assign.target_uninitialized) \
            prototype.func = type->init_assign; \
          else \
            prototype.func = type->assign; } while(0)
#define HANDLE_RVALUE_STACK() \
          prototype.src_offset = context->vars[rvalue->var].offset
#define HANDLE_RVALUE_POINTER() \
          prototype.src_offset = context->vars[rvalue->var].offset; \
          prototype.src_ptr_offset = rvalue->offset
#define HANDLE_RVALUE_GLOBAL() \
          prototype.src_ns = rvalue->ns; \
          prototype.src_offset = rvalue->offset
#define HANDLE_RVALUE_LITERAL() \
          prototype.src_value = dang_value_copy (rvalue->type, rvalue->value); \
          dang_insn_pack_context_add_destroy (context, (DangInsnDestroyNotify) dang_value_destroy, \
                                              rvalue->type, prototype.src_value)
#define HANDLE_MEMCPY() \
          prototype.size = type->sizeof_instance
#define ADD_STEP(Suffix, suffix) \
  dang_insn_pack_context_append (context, assign_##suffix, sizeof (prototype), &prototype, NULL)
  switch (LRVALUE (lvalue->location, rvalue->location))
    {
    case LRVALUE_S(STACK, STACK):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy, memcpy);
          HANDLE_LVALUE_STACK ();
          HANDLE_RVALUE_STACK ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy, memcpy);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual, virtual);
          HANDLE_LVALUE_STACK ();
          HANDLE_RVALUE_STACK ();
          HANDLE_VIRTUAL__LVALUE_STACK ();
          ADD_STEP (Virtual, virtual);
        }
      break;

    case LRVALUE_S(GLOBAL, STACK):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Lglobal, memcpy_lglobal);
          HANDLE_LVALUE_GLOBAL ();
          HANDLE_RVALUE_STACK ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Lglobal, memcpy_lglobal);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Lglobal, virtual_lglobal);
          HANDLE_LVALUE_GLOBAL ();
          HANDLE_RVALUE_STACK ();
          HANDLE_VIRTUAL ();
          ADD_STEP (Virtual_Lglobal, virtual_lglobal);
        }
      break;

    case LRVALUE_S(POINTER, STACK):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Lptr, memcpy_lptr);
          HANDLE_LVALUE_POINTER ();
          HANDLE_RVALUE_STACK ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Lptr, memcpy_lptr);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Lptr, virtual_lptr);
          HANDLE_LVALUE_POINTER ();
          HANDLE_RVALUE_STACK ();
          HANDLE_VIRTUAL ();
          ADD_STEP (Virtual_Lptr, virtual_lptr);
        }
      break;

    case LRVALUE_S(STACK, GLOBAL):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Rglobal, memcpy_rglobal);
          HANDLE_LVALUE_STACK ();
          HANDLE_RVALUE_GLOBAL ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Rglobal, memcpy_rglobal);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Rglobal, virtual_rglobal);
          HANDLE_LVALUE_STACK ();
          HANDLE_RVALUE_GLOBAL ();
          HANDLE_VIRTUAL__LVALUE_STACK ();
          ADD_STEP (Virtual_Rglobal, virtual_rglobal);
        }
      break;
    case LRVALUE_S(GLOBAL, GLOBAL):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Lglobal_Rglobal, memcpy_lglobal_rglobal);
          HANDLE_LVALUE_GLOBAL ();
          HANDLE_RVALUE_GLOBAL ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Lglobal_Rglobal, memcpy_lglobal_rglobal);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Lglobal_Rglobal, virtual_lglobal_rglobal);
          HANDLE_LVALUE_GLOBAL ();
          HANDLE_RVALUE_GLOBAL ();
          HANDLE_VIRTUAL ();
          ADD_STEP (Virtual_Lglobal_Rglobal, virtual_lglobal_rglobal);
        }
      break;
    case LRVALUE_S(POINTER, GLOBAL):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Lptr_Rglobal, memcpy_lptr_rglobal);
          HANDLE_LVALUE_POINTER ();
          HANDLE_RVALUE_GLOBAL ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Lptr_Rglobal, memcpy_lptr_rglobal);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Lptr_Rglobal, virtual_lptr_rglobal);
          HANDLE_LVALUE_POINTER ();
          HANDLE_RVALUE_GLOBAL ();
          HANDLE_VIRTUAL ();
          ADD_STEP (Virtual_Lptr_Rglobal, virtual_lptr_rglobal);
        }
      break;

    case LRVALUE_S(STACK, POINTER):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Rptr, memcpy_rptr);
          HANDLE_LVALUE_STACK ();
          HANDLE_RVALUE_POINTER ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Rptr, memcpy_rptr);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Rptr, virtual_rptr);
          HANDLE_LVALUE_STACK ();
          HANDLE_RVALUE_POINTER ();
          HANDLE_VIRTUAL__LVALUE_STACK ();
          ADD_STEP (Virtual_Rptr, virtual_rptr);
        }
      break;
    case LRVALUE_S(GLOBAL, POINTER):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Lglobal_Rptr, memcpy_lglobal_rptr);
          HANDLE_LVALUE_GLOBAL ();
          HANDLE_RVALUE_POINTER ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Lglobal_Rptr, memcpy_lglobal_rptr);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Lglobal_Rptr, virtual_lglobal_rptr);
          HANDLE_LVALUE_GLOBAL ();
          HANDLE_RVALUE_POINTER ();
          HANDLE_VIRTUAL ();
          ADD_STEP (Virtual_Lglobal_Rptr, virtual_lglobal_rptr);
        }
      break;

    case LRVALUE_S(POINTER, POINTER):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Lptr_Rptr, memcpy_lptr_rptr);
          HANDLE_LVALUE_POINTER ();
          HANDLE_RVALUE_POINTER ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Lptr_Rptr, memcpy_lptr_rptr);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Lptr_Rptr, virtual_lptr_rptr);
          HANDLE_LVALUE_POINTER ();
          HANDLE_RVALUE_POINTER ();
          HANDLE_VIRTUAL ();
          ADD_STEP (Virtual_Lptr_Rptr, virtual_lptr_rptr);
        }
      break;

    case LRVALUE_S(STACK, LITERAL):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Rliteral, memcpy_rliteral);
          HANDLE_LVALUE_STACK ();
          HANDLE_RVALUE_LITERAL ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Rliteral, memcpy_rliteral);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Rliteral, virtual_rliteral);
          HANDLE_LVALUE_STACK ();
          HANDLE_RVALUE_LITERAL ();
          HANDLE_VIRTUAL__LVALUE_STACK ();
          ADD_STEP (Virtual_Rliteral, virtual_rliteral);
        }
      break;
    case LRVALUE_S(GLOBAL, LITERAL):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Lglobal_Rliteral, memcpy_lglobal_rliteral);
          HANDLE_LVALUE_GLOBAL ();
          HANDLE_RVALUE_LITERAL ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Lglobal_Rliteral, memcpy_lglobal_rliteral);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Lglobal_Rliteral, virtual_lglobal_rliteral);
          HANDLE_LVALUE_GLOBAL ();
          HANDLE_RVALUE_LITERAL ();
          HANDLE_VIRTUAL ();
          ADD_STEP (Virtual_Lglobal_Rliteral, virtual_lglobal_rliteral);
        }
      break;
    case LRVALUE_S(POINTER, LITERAL):
      if (use_memcpy)
        {
          DECLARE_AND_INIT_PROTOTYPE (Memcpy_Lptr_Rliteral, memcpy_lptr_rliteral);
          HANDLE_LVALUE_POINTER ();
          HANDLE_RVALUE_LITERAL ();
          HANDLE_MEMCPY ();
          ADD_STEP (Memcpy_Lptr_Rliteral, memcpy_lptr_rliteral);
        }
      else
        {
          DECLARE_AND_INIT_PROTOTYPE (Virtual_Lptr_Rliteral, virtual_lptr_rliteral);
          HANDLE_LVALUE_POINTER ();
          HANDLE_RVALUE_LITERAL ();
          HANDLE_VIRTUAL ();
          ADD_STEP (Virtual_Lptr_Rliteral, virtual_lptr_rliteral);
        }
      break;

    default:
      dang_assert_not_reached ();
    }
}

/* === GOTO, GOTO_CONDITIONAL === */
typedef struct {
  DangStep *target;             /* must be first */
} GotoRunData_Unconditional;
typedef struct {
  DangStep *target;             /* must be first */
  unsigned offset, size;
} GotoRunData_Stack;
typedef struct {
  DangStep *target;             /* must be first */
  DangNamespace *ns;
  unsigned offset, size;
} GotoRunData_Global;
typedef struct {
  DangStep *target;             /* must be first */
  unsigned ptr, offset, size;
} GotoRunData_Pointer;

static void
step__jump (void                 *step_data,
          DangThreadStackFrame *stack_frame,
          DangThread           *thread)
{
  GotoRunData_Unconditional *rd = step_data;
  DANG_UNUSED (thread);
  stack_frame->ip = rd->target;
}


#define PREAMBLE_GLOBAL() \
  GotoRunData_Global *rd = step_data; \
  void *ptr = rd->ns->global_data + rd->offset; \
  DANG_UNUSED (thread);
#define PREAMBLE_STACK() \
  GotoRunData_Stack *rd = step_data; \
  void *ptr = (char*)stack_frame + rd->offset; \
  DANG_UNUSED (thread);
#define PREAMBLE_POINTER() \
  GotoRunData_Pointer *rd = step_data; \
  void *ptr = * (void**) ((char*)stack_frame + rd->ptr); \
  if (ptr == NULL) \
    { \
      dang_thread_throw_null_pointer_exception (thread); \
      return; \
    } \
  ptr = (char*)ptr + rd->offset;


#define POSTAMBLE_JUMP_IF_ZERO() \
  if (dang_util_is_zero (ptr, rd->size))      \
    stack_frame->ip = rd->target; \
  else \
    dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*rd));
#define POSTAMBLE_JUMP_IF_NONZERO() \
  if (dang_util_is_zero (ptr, rd->size))      \
    dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*rd)); \
  else \
    stack_frame->ip = rd->target;
static void
step__jump_if_zero_global (void                 *step_data,
                         DangThreadStackFrame *stack_frame,
                         DangThread           *thread)
{
  PREAMBLE_GLOBAL ();
  POSTAMBLE_JUMP_IF_ZERO ();
}

static void
step__jump_if_nonzero_global (void                 *step_data,
                            DangThreadStackFrame *stack_frame,
                            DangThread           *thread)
{
  PREAMBLE_GLOBAL ();
  POSTAMBLE_JUMP_IF_NONZERO ();
}
static void
step__jump_if_zero_stack  (void                 *step_data,
                         DangThreadStackFrame *stack_frame,
                         DangThread           *thread)
{
  PREAMBLE_STACK ();
  POSTAMBLE_JUMP_IF_ZERO ();
}
static void
step__jump_if_nonzero_stack  (void                 *step_data,
                            DangThreadStackFrame *stack_frame,
                            DangThread           *thread)
{
  PREAMBLE_STACK ();
  POSTAMBLE_JUMP_IF_NONZERO ();
}
static void
step__jump_if_zero_pointer(void                 *step_data,
                         DangThreadStackFrame *stack_frame,
                         DangThread           *thread)
{
  PREAMBLE_POINTER ();
  POSTAMBLE_JUMP_IF_ZERO ();
}
static void
step__jump_if_nonzero_pointer(void                 *step_data,
                            DangThreadStackFrame *stack_frame,
                            DangThread           *thread)
{
  PREAMBLE_POINTER ();
  POSTAMBLE_JUMP_IF_NONZERO ();
}

typedef struct {
  DangStep *target;             /* must be first */
  unsigned offset;
} GotoRunData_Stack1;
static void
step__jump_if_zero_stack1 (void                 *step_data,
                         DangThreadStackFrame *stack_frame,
                         DangThread           *thread)
{
  GotoRunData_Stack1 *rd = step_data;
  DANG_UNUSED (thread);
  if (((char*) stack_frame)[rd->offset])
    dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*rd));
  else
    stack_frame->ip = rd->target;
}
static void
step__jump_if_nonzero_stack1 (void                 *step_data,
                            DangThreadStackFrame *stack_frame,
                            DangThread           *thread)
{
  GotoRunData_Stack1 *rd = step_data;
  DANG_UNUSED (thread);
  if (((char*) stack_frame)[rd->offset])
    stack_frame->ip = rd->target;
  else
    dang_thread_stack_frame_advance_ip (stack_frame, sizeof (*rd));
}


#define APPEND_RUN_DATA(func, run_data) \
  dang_insn_pack_context_append (context,      \
                                 func, sizeof (run_data), &run_data, \
                                 NULL)

static void
add_unconditional_jump (DangInsnPackContext *context,
                        DangLabelId target)
{
  GotoRunData_Unconditional rd;
  dang_insn_pack_context_note_target (context, target, 0);
  APPEND_RUN_DATA (step__jump, rd);
}
static void
pack__jump (DangInsn *insn,
            DangInsnPackContext *context)
{
  add_unconditional_jump (context, insn->jump.target);
}

static void
pack__jump_conditional (DangInsn *insn,
                        DangInsnPackContext *context)
{
  DangInsnValue *v = &insn->jump_conditional.test_value;
  dang_boolean jump_if_zero = insn->jump_conditional.jump_if_zero;
  if (v->location == DANG_INSN_LOCATION_LITERAL)
    {
      dang_boolean jump;
      if (dang_util_is_zero (v->value, v->type->sizeof_instance))
        jump = jump_if_zero;
      else
        jump = !jump_if_zero;
      if (jump)
        add_unconditional_jump (context, insn->jump_conditional.target);
      return;
    }
  dang_insn_pack_context_note_target (context, insn->jump_conditional.target, 0);
  switch (v->location)
    {
    case DANG_INSN_LOCATION_STACK:
      {
        DangValueType *type = context->vars[v->var].type;
        if (type->sizeof_instance == 1)
          {
            GotoRunData_Stack1 rd;
            rd.offset = context->vars[v->var].offset;
            if (jump_if_zero)
              APPEND_RUN_DATA (step__jump_if_zero_stack1, rd);
            else
              APPEND_RUN_DATA (step__jump_if_nonzero_stack1, rd);
          }
        else
          {
            GotoRunData_Stack rd;
            rd.offset = context->vars[v->var].offset;
            rd.size = type->sizeof_instance;
            if (jump_if_zero)
              APPEND_RUN_DATA (step__jump_if_zero_stack, rd);
            else
              APPEND_RUN_DATA (step__jump_if_nonzero_stack, rd);
          }
      }
      break;
    case DANG_INSN_LOCATION_POINTER:
      {
        GotoRunData_Pointer rd;
        rd.ptr = context->vars[v->var].offset;
        rd.offset = v->offset;
        rd.size = v->type->sizeof_instance;
        if (jump_if_zero)
          APPEND_RUN_DATA (step__jump_if_zero_pointer, rd);
        else
          APPEND_RUN_DATA (step__jump_if_nonzero_pointer, rd);
      }
      break;
    case DANG_INSN_LOCATION_GLOBAL:
      {
        GotoRunData_Global rd;
        rd.ns = v->ns;
        rd.offset = v->offset;
        rd.size = v->type->sizeof_instance;
        if (jump_if_zero)
          APPEND_RUN_DATA (step__jump_if_zero_global, rd);
        else
          APPEND_RUN_DATA (step__jump_if_nonzero_global, rd);
      }
      break;
    default:
      dang_assert_not_reached ();
    }
}

/* === FUNCTION_CALL === */
/* Compiling a generic function invocation
   requires 2 DangIntermediateSteps:
   Step 1:
   - Check any pointer results for null-ptr exceptions.
   - Allocate a new frame.  We are not allowed to
     throw exceptions until we have pushed the frame onto the stack,
     to avoid a memory leak.
   - We use a sequence of specialized steps
     to initialize the frame.
   - Push the frame onto the thread's stack, first advancing the
     return code pointer, so that on return we will be at Step 2.
   - We allocate a local variable for the return frame.
   Step 2:
   - Handle output parameters (including the return-value)
   - Free the frame.
   We will never get to Step 2 if an exception is thrown.
 */

/* Both step 1 and 2 contain a series of substeps.
 */
typedef enum
{
  INVOCATION_INPUT_SUBSTEP_ZERO,
  /* NOTE! we depend on (VIRTUAL_X == MEMCPY_X + 1).  ugh! */
  INVOCATION_INPUT_SUBSTEP_MEMCPY_STACK,
  INVOCATION_INPUT_SUBSTEP_VIRTUAL_STACK,
  INVOCATION_INPUT_SUBSTEP_MEMCPY_POINTER,
  INVOCATION_INPUT_SUBSTEP_VIRTUAL_POINTER,
  INVOCATION_INPUT_SUBSTEP_MEMCPY_GLOBAL,
  INVOCATION_INPUT_SUBSTEP_VIRTUAL_GLOBAL,
  INVOCATION_INPUT_SUBSTEP_MEMCPY_LITERAL,
  INVOCATION_INPUT_SUBSTEP_VIRTUAL_LITERAL,
} InvocationInputSubstepType;

typedef struct _InvocationInputSubstep InvocationInputSubstep;
struct _InvocationInputSubstep
{
  InvocationInputSubstepType type;
  /* Offset of the target into the called frame. */
  unsigned called_offset;
  union {
    unsigned size;              /* for ZERO and MEMCPY types */
    DangValueType *type;        /* for VIRTUAL types */
  } type_info;
  union {
    struct { unsigned src_offset; } stack;
    struct { unsigned ptr,offset; } pointer;
    struct { DangNamespace *ns; unsigned ns_offset; } global;
    struct { unsigned offset; } literal;

  } info;
};

typedef struct _InvocationInputInfo InvocationInputInfo;
struct _InvocationInputInfo
{
  unsigned return_frame_offset;
  unsigned n_pointers;
  unsigned n_steps;
  dang_boolean is_literal_function;
  union {
    DangFunction *literal;
    unsigned offset;            /* a pointer to a function on the stack */
  } func;
  unsigned sizeof_step_data;

  /* Followed by unsigned[n_pointers]
       -- the offsets of pointers to check for a null-ptr exception
     Then n_steps
       -- the InvocationInputSubsteps to start the thing.
   */
};

static void
run_compiled_function_invocation (void                 *step_data,
                                  DangThreadStackFrame *stack_frame,
                                  DangThread           *thread)
{
  InvocationInputInfo *iii = step_data;
  unsigned *null_check_ptr_offsets = (unsigned *)(iii+1);
  unsigned n_steps;
  char *frame = (char*)stack_frame;
  unsigned i;
  InvocationInputSubstep *substeps;
  char *new_frame;
  DangThreadStackFrame *new;
  DangFunction *function;
  if (iii->is_literal_function)
    function = iii->func.literal;
  else
    {
      function = * (DangFunction**) ((char*)stack_frame + iii->func.offset);
      if (function == NULL)
        {
          dang_thread_throw_null_pointer_exception (thread);
          return;
        }
    }
  for (i = 0; i < iii->n_pointers; i++)
    {
      if (* (void **) (frame + *null_check_ptr_offsets) == NULL)
        {
          dang_thread_throw_null_pointer_exception (thread);
          return;
        }
      null_check_ptr_offsets++;
    }
  substeps = (InvocationInputSubstep *) (null_check_ptr_offsets);
  n_steps = iii->n_steps;
  new_frame = dang_malloc (function->base.frame_size);
  for (i = 0; i < n_steps; i++)
    {
      char *ptr;
      InvocationInputSubstep *substep = substeps + i;
      switch (substep->type)
        {
        case INVOCATION_INPUT_SUBSTEP_ZERO:
          memset (new_frame + substep->called_offset, 0,
                  substep->type_info.size);
          break;
        case INVOCATION_INPUT_SUBSTEP_MEMCPY_STACK:
          memcpy (new_frame + substep->called_offset,
                  frame + substep->info.stack.src_offset,
                  substep->type_info.size);
          break;
        case INVOCATION_INPUT_SUBSTEP_VIRTUAL_STACK:
          substep->type_info.type->init_assign
                 (substep->type_info.type,
                  new_frame + substep->called_offset,
                  frame + substep->info.stack.src_offset);
          break;
        case INVOCATION_INPUT_SUBSTEP_MEMCPY_POINTER:
          ptr = * ((char **) (frame + substep->info.pointer.ptr));
          memcpy (new_frame + substep->called_offset,
                  ptr + substep->info.pointer.offset,
                  substep->type_info.size);
          break;
        case INVOCATION_INPUT_SUBSTEP_VIRTUAL_POINTER:
          ptr = * ((char **) (frame + substep->info.pointer.ptr));
          substep->type_info.type->init_assign
                 (substep->type_info.type,
                  new_frame + substep->called_offset,
                  ptr + substep->info.pointer.offset);
          break;
        case INVOCATION_INPUT_SUBSTEP_MEMCPY_GLOBAL:
          memcpy (new_frame + substep->called_offset,
                  substep->info.global.ns->global_data + substep->info.global.ns_offset,
                  substep->type_info.size);
          break;
        case INVOCATION_INPUT_SUBSTEP_VIRTUAL_GLOBAL:
          substep->type_info.type->init_assign
                 (substep->type_info.type,
                  new_frame + substep->called_offset,
                  substep->info.global.ns->global_data + substep->info.global.ns_offset);
          break;
        case INVOCATION_INPUT_SUBSTEP_MEMCPY_LITERAL:
          memcpy (new_frame + substep->called_offset,
                  (char*)step_data + substep->info.literal.offset,
                  substep->type_info.size);
          break;
        case INVOCATION_INPUT_SUBSTEP_VIRTUAL_LITERAL:
          substep->type_info.type->init_assign
                 (substep->type_info.type,
                  new_frame + substep->called_offset,
                  (char*)step_data + substep->info.literal.offset);
          break;
        }
    }

  /* Advance our instruction pointer */
  dang_thread_stack_frame_advance_ip (stack_frame, iii->sizeof_step_data);

  /* push stack frame */
  new = (DangThreadStackFrame *) new_frame;
  new->caller = stack_frame;
  new->function = function;
  new->ip = function->base.steps;
  thread->stack_frame = new;
  
  /* tuck the frame pointer away for when we need it. */
  * (DangThreadStackFrame **) (frame + iii->return_frame_offset) = new;
}

static void
invocation_input_info_destruct (void *step_data)
{
  InvocationInputInfo *iii = step_data;
  unsigned *ptrs = (unsigned*)(iii+1);
  InvocationInputSubstep *substeps = (InvocationInputSubstep*)(ptrs + iii->n_pointers);
  unsigned n_substeps = iii->n_steps;
  unsigned i;
  for (i = 0; i < n_substeps; i++)
    if (substeps[i].type == INVOCATION_INPUT_SUBSTEP_VIRTUAL_LITERAL)
      substeps[i].type_info.type->destruct (substeps[i].type_info.type,
                                            (char*)step_data + substeps[i].info.literal.offset);
}
//static dang_boolean
//invocation_input_info_patch (DangBuilder  *builder,
//                             DangIntermediateStep *step_inout,
//                             DangError           **error)
//{
//  InvocationInputInfo *iii = step_inout->step_data;
//  unsigned *ptrs = (unsigned*)(iii+1);
//  InvocationInputSubstep *substeps = (InvocationInputSubstep*)(ptrs + iii->n_pointers);
//  unsigned i;
//  DANG_UNUSED (error);
//  if (!iii->is_literal_function)
//    dang_builder_patch_var_to_offset (builder, &iii->func.offset);
//  for (i = 0; i < iii->n_pointers; i++)
//    dang_builder_patch_var_to_offset (builder, &ptrs[i]);
//  dang_builder_patch_var_to_offset (builder, &iii->return_frame_offset);
//  for (i = 0; i < iii->n_steps; i++)
//    switch (substeps[i].type)
//      {
//      case INVOCATION_INPUT_SUBSTEP_MEMCPY_STACK:
//      case INVOCATION_INPUT_SUBSTEP_VIRTUAL_STACK:
//        dang_builder_patch_var_to_offset (builder, &substeps[i].info.stack.src_offset);
//        break;
//      case INVOCATION_INPUT_SUBSTEP_MEMCPY_POINTER:
//      case INVOCATION_INPUT_SUBSTEP_VIRTUAL_POINTER:
//        dang_builder_patch_var_to_offset (builder, &substeps[i].info.pointer.ptr);
//        break;
//      default:
//        break;
//      }
//  return TRUE;
//}

typedef enum
{
  INVOCATION_OUTPUT_SUBSTEP_DESTRUCT,
  /* NOTE! we depend on (VIRTUAL_X == MEMCPY_X + 1).  ugh! */
  INVOCATION_OUTPUT_SUBSTEP_MEMCPY_STACK,
  INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_STACK,
  INVOCATION_OUTPUT_SUBSTEP_MEMCPY_POINTER,
  INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_POINTER,
  INVOCATION_OUTPUT_SUBSTEP_MEMCPY_GLOBAL,
  INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_GLOBAL,
} InvocationOutputSubstepType;

typedef struct _InvocationOutputSubstep InvocationOutputSubstep;
struct _InvocationOutputSubstep
{
  InvocationOutputSubstepType type;
  /* Offset of the target into the called frame. */
  unsigned called_offset;
  union {
    unsigned size;              /* for MEMCPY types */
    DangValueType *type;        /* for VIRTUAL and DESTRUCT types */
  } type_info;
  union {
    struct { unsigned dst_offset; } stack;
    struct { unsigned ptr,offset; } pointer;
    struct { DangNamespace *ns; unsigned ns_offset; } global;
  } info;
};

typedef struct _InvocationOutputInfo InvocationOutputInfo;
struct _InvocationOutputInfo
{
  unsigned return_frame_offset;
  unsigned n_steps;
  
  /* Then n_steps InvocationOutputSubstep */
};

static void
run_compiled_function_output (void                 *step_data,
                              DangThreadStackFrame *stack_frame,
                              DangThread           *thread)
{
  InvocationOutputInfo *ioi = step_data;
  InvocationOutputSubstep *substeps = (InvocationOutputSubstep*)(ioi + 1);
  unsigned i, n_steps = ioi->n_steps;
  DangThreadStackFrame *called_frame = * (DangThreadStackFrame**) ((char*)stack_frame + ioi->return_frame_offset);
  void *src;
  DangValueType *type;
  unsigned size;
  void *ptr;
  dang_boolean throw_null_pointer_exception = FALSE;
  for (i = 0; i < n_steps; i++)
    {
      src = ((char*)called_frame + substeps[i].called_offset);
      switch (substeps[i].type)
        {
        case INVOCATION_OUTPUT_SUBSTEP_DESTRUCT:
          type = substeps[i].type_info.type;
          type->destruct (type, (char*)called_frame + substeps[i].called_offset);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_MEMCPY_STACK:
          size = substeps[i].type_info.size;
          memcpy ((char*)stack_frame + substeps[i].info.stack.dst_offset,
                  src, size);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_STACK:
          type = substeps[i].type_info.type;
          type->destruct (type, (char*)stack_frame + substeps[i].info.stack.dst_offset);
          memcpy ((char*)stack_frame + substeps[i].info.stack.dst_offset, src, type->sizeof_instance);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_MEMCPY_POINTER:
          size = substeps[i].type_info.size;
          ptr = * (char **) ((char*)stack_frame + substeps[i].info.pointer.ptr);
          if (ptr == NULL)
            throw_null_pointer_exception = TRUE;
          else
            memcpy ((char*) ptr + substeps[i].info.pointer.offset, src, size);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_POINTER:
          type = substeps[i].type_info.type;
          ptr = * (char **) ((char*)stack_frame + substeps[i].info.pointer.ptr);
          if (ptr == NULL)
            {
              throw_null_pointer_exception = TRUE;
              type->destruct (type, src);
            }
          else
            {
              type->destruct (type, (char*) ptr + substeps[i].info.pointer.offset);
              memcpy ((char*) ptr + substeps[i].info.pointer.offset, src, type->sizeof_instance);
            }
          break;
        case INVOCATION_OUTPUT_SUBSTEP_MEMCPY_GLOBAL:
          size = substeps[i].type_info.size;
          ptr = substeps[i].info.global.ns->global_data + substeps[i].info.global.ns_offset;
          memcpy (ptr, src, size);
          break;
        case INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_GLOBAL:
          type = substeps[i].type_info.type;
          ptr = substeps[i].info.global.ns->global_data + substeps[i].info.global.ns_offset;
          type->destruct (type, ptr);
          memcpy (ptr, src, type->sizeof_instance);
          break;
        }
    }

  dang_free (called_frame);

  if (throw_null_pointer_exception)
    dang_thread_throw_null_pointer_exception (thread);

  dang_thread_stack_frame_advance_ip (stack_frame,
                                      sizeof(InvocationOutputInfo)
                                      + sizeof(InvocationOutputSubstep)*n_steps);
}

///static dang_boolean
///invocation_output_info_patch (DangBuilder  *builder,
///                              DangIntermediateStep *step_inout,
///                              DangError           **error)
///{
///  InvocationOutputInfo *ioi = step_inout->step_data;
///  InvocationOutputSubstep *substeps = (InvocationOutputSubstep*)(ioi+1);
///  DangBuilderVariable *vars = builder->vars.data;
///  unsigned i;
///  DANG_UNUSED (error);
///  ioi->return_frame_offset = vars[ioi->return_frame_offset].offset;
///  for (i = 0; i < ioi->n_steps; i++)
///    switch (substeps[i].type)
///      {
///      case INVOCATION_OUTPUT_SUBSTEP_MEMCPY_STACK:
///      case INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_STACK:
///        substeps[i].info.stack.dst_offset = vars[substeps[i].info.stack.dst_offset].offset;
///        break;
///      case INVOCATION_OUTPUT_SUBSTEP_MEMCPY_POINTER:
///      case INVOCATION_OUTPUT_SUBSTEP_VIRTUAL_POINTER:
///        substeps[i].info.pointer.ptr = vars[substeps[i].info.pointer.ptr].offset;
///        break;
///      default:
///        break;
///      }
///  return TRUE;
///}

static void
handle_param_substeps (DangInsnPackContext *context,
                       DangArray           *pointers,
                       DangArray           *input_substeps,
                       DangArray           *output_substeps,
                       DangArray           *value_data,
                       DangInsnValue      *res,
                       DangFunctionParamDir dir,
                       unsigned             called_offset)
{
  InvocationInputSubstep istep;
  InvocationOutputSubstep ostep;
  istep.called_offset = called_offset;
  ostep.called_offset = called_offset;
//  if (dir == DANG_FUNCTION_PARAM_OUT || !has_actual)
//    {
//      /* Just zero the param on input */
//      istep.type = INVOCATION_INPUT_SUBSTEP_ZERO;
//      istep.type_info.size = type->sizeof_instance;
//      dang_util_array_append (input_substeps, 1, &istep);
//    }
//  else
    {
      dang_boolean use_memcpy = res->type->init_assign == NULL;
      if (res->type->init_assign == NULL)
        istep.type_info.size = res->type->sizeof_instance;
      else
        istep.type_info.type = res->type;
      switch (res->location)
        {
           /* note that MEMCPY will be converted to VIRTUAL
              as needed by merely adding one! (after the case) */
        case DANG_INSN_LOCATION_STACK:
          istep.type = INVOCATION_INPUT_SUBSTEP_MEMCPY_STACK;
          istep.info.stack.src_offset = context->vars[res->var].offset;
          break;
        case DANG_INSN_LOCATION_POINTER:
          {
            unsigned offset = context->vars[res->var].offset;
            dang_util_array_append (pointers, 1, &offset);
            istep.type = INVOCATION_INPUT_SUBSTEP_MEMCPY_POINTER;
            istep.info.pointer.ptr = context->vars[res->var].offset;
            istep.info.pointer.offset = res->offset;
            break;
          }
        case DANG_INSN_LOCATION_GLOBAL:
          istep.type = INVOCATION_INPUT_SUBSTEP_MEMCPY_GLOBAL;
          istep.info.global.ns = res->ns;
          istep.info.global.ns_offset = res->offset;
          break;
        case DANG_INSN_LOCATION_LITERAL:
          istep.type = INVOCATION_INPUT_SUBSTEP_MEMCPY_LITERAL;

          /* Align value_data */
          if (value_data->len % res->type->alignof_instance != 0)
            {
              unsigned needed = res->type->alignof_instance
                              - value_data->len % res->type->alignof_instance;
              dang_util_array_set_size (value_data, value_data->len + needed);
              memset (value_data->data + value_data->len - needed, 0, needed);
            }

          istep.info.literal.offset = value_data->len;
          dang_util_array_set_size (value_data,
                               value_data->len + res->type->sizeof_instance);
          {
            void *dst = value_data->data + value_data->len - res->type->sizeof_instance;
            if (res->type->init_assign)
              res->type->init_assign (res->type, dst, res->value);
            else
              memcpy (dst, res->value, res->type->sizeof_instance);
          }
          break;
        default:
          dang_assert_not_reached ();
        }

      /* Convert MEMCPY_X to VIRTUAL_X */
      if (!use_memcpy)
        istep.type += 1;

      dang_util_array_append (input_substeps, 1, &istep);
    }

  if (dir == DANG_FUNCTION_PARAM_OUT
   || dir == DANG_FUNCTION_PARAM_INOUT)
    {
      dang_boolean use_memcpy = (res->type->init_assign == NULL);
      if (use_memcpy)
        ostep.type_info.size = res->type->sizeof_instance;
      else
        ostep.type_info.type = res->type;
      switch (res->location)
        {
        case DANG_INSN_LOCATION_STACK:
          ostep.type = INVOCATION_OUTPUT_SUBSTEP_MEMCPY_STACK;
          ostep.info.stack.dst_offset = context->vars[res->var].offset;
          break;
        case DANG_INSN_LOCATION_POINTER:
          ostep.type = INVOCATION_OUTPUT_SUBSTEP_MEMCPY_POINTER;
          ostep.info.pointer.ptr = context->vars[res->var].offset;
          ostep.info.pointer.offset = res->offset;
          break;
        case DANG_INSN_LOCATION_GLOBAL:
          ostep.type = INVOCATION_OUTPUT_SUBSTEP_MEMCPY_GLOBAL;
          ostep.info.global.ns = res->ns;
          ostep.info.global.ns_offset = res->offset;
          break;
        default:
          dang_assert_not_reached ();
        }

      /* Convert MEMCPY_X to VIRTUAL_X */
      if (!use_memcpy)
        ostep.type += 1;

      dang_util_array_append (output_substeps, 1, &ostep);
    }
  else if (res->type->destruct != NULL)
    {
      /* add destructor */
      ostep.type = INVOCATION_OUTPUT_SUBSTEP_DESTRUCT;
      ostep.type_info.type = res->type;
      dang_util_array_append (output_substeps, 1, &ostep);
    }
}

static inline void
align_offset (unsigned *offset_inout, DangValueType *type)
{
  *offset_inout = DANG_ALIGN (*offset_inout, type->alignof_instance);
}


static void
pack__function_call (DangInsn *insn,
                     DangInsnPackContext *context)
{
  InvocationInputInfo *input_info;
  InvocationOutputInfo *output_info;
  DangFunctionParam *fparams;
  DangArray pointers, input_substeps, output_substeps, value_data;
  unsigned i;
  unsigned iii_size, ioi_size;
  unsigned cur_called_offset;
  char *at;
  DangFunction *function;
  DangSignature *sig;
  DangInsnValue *function_res = &insn->function_call.function;
  dang_assert (dang_value_type_is_function (function_res->type));
  if (function_res->location == DANG_INSN_LOCATION_LITERAL)
    function = * (DangFunction **) function_res->value;
  else
    {
      dang_assert (function_res->location == DANG_INSN_LOCATION_STACK);
      function = NULL;
    }
  sig = ((DangValueTypeFunction*)function_res->type)->sig;


  /* Compute parameter details */
  DANG_UTIL_ARRAY_INIT (&pointers, unsigned);
  DANG_UTIL_ARRAY_INIT (&input_substeps, InvocationInputSubstep);
  DANG_UTIL_ARRAY_INIT (&output_substeps, InvocationOutputSubstep);
  DANG_UTIL_ARRAY_INIT (&value_data, char);
  fparams = sig->params;
  cur_called_offset = sizeof (DangThreadStackFrame);
  if (sig->return_type != NULL)
    {
      align_offset (&cur_called_offset, sig->return_type);
      handle_param_substeps (context,
                             &pointers, &input_substeps, &output_substeps, &value_data,
                             insn->function_call.params + 0,
                             DANG_FUNCTION_PARAM_OUT, cur_called_offset);
      cur_called_offset += sig->return_type->sizeof_instance;
    }
  for (i = 0; i < sig->n_params; i++)
    {
      align_offset (&cur_called_offset, fparams[i].type);
      handle_param_substeps (context, &pointers,
                             &input_substeps, &output_substeps,
                             &value_data,
                             insn->function_call.params + i
                              + (sig->return_type ? 1 : 0),
                             fparams[i].dir, cur_called_offset);
      cur_called_offset += fparams[i].type->sizeof_instance;
    }

  /* Step 1 */
  iii_size = sizeof (InvocationInputInfo)
           + sizeof (unsigned) * pointers.len
           + sizeof (InvocationInputSubstep) * input_substeps.len
           + value_data.len;

  /* fixup literal offsets */
  for (i = 0; i < input_substeps.len; i++)
    {
      InvocationInputSubstep *s = (InvocationInputSubstep*)input_substeps.data + i;
      if (s->type == INVOCATION_INPUT_SUBSTEP_MEMCPY_LITERAL
       || s->type == INVOCATION_INPUT_SUBSTEP_VIRTUAL_LITERAL)
        s->info.literal.offset +=
                   sizeof (InvocationInputInfo)
                 + sizeof (unsigned) * pointers.len
                 + sizeof (InvocationInputSubstep) * input_substeps.len;
    }

  input_info = dang_malloc (iii_size);
  input_info->return_frame_offset = context->vars[insn->function_call.frame_var_id].offset;
  input_info->n_pointers = pointers.len;
  input_info->n_steps = input_substeps.len;
  if (function == NULL)
    {
      input_info->is_literal_function = FALSE;
      dang_assert (function_res->location == DANG_INSN_LOCATION_STACK);
      input_info->func.offset = context->vars[function_res->var].offset;
    }
  else
    {
      input_info->is_literal_function = TRUE;
      if (function->base.is_owned)
        {

          input_info->func.literal = function;
        }
      else
        {
          input_info->func.literal = dang_function_ref (function);
          dang_insn_pack_context_add_destroy (context, (DangInsnDestroyNotify) dang_function_unref, function, NULL);
        }
    }
  input_info->sizeof_step_data = iii_size;
  at = (char*)(input_info + 1);
  memcpy (at, pointers.data, pointers.len * sizeof (unsigned));
  at += pointers.len * sizeof (unsigned);
  memcpy (at, input_substeps.data, input_substeps.len * sizeof (InvocationInputSubstep));
  at += sizeof (InvocationInputSubstep) * input_substeps.len;
  memcpy (at, value_data.data, value_data.len);

  dang_insn_pack_context_append (context, run_compiled_function_invocation,
                                 iii_size, input_info,
                                 invocation_input_info_destruct);
  dang_free (input_info);

  /* Step 2. */
  ioi_size = sizeof (InvocationOutputInfo)
           + sizeof (InvocationOutputSubstep) * output_substeps.len;
  output_info = dang_malloc (ioi_size);
  output_info->return_frame_offset = context->vars[insn->function_call.frame_var_id].offset;
  output_info->n_steps = output_substeps.len;
  memcpy (output_info + 1, output_substeps.data,
          output_substeps.len * sizeof (InvocationOutputSubstep));
  dang_insn_pack_context_append (context, run_compiled_function_output,
                                 ioi_size, output_info,
                                 NULL);
  dang_free (output_info);

  dang_util_array_clear (&pointers);
  dang_util_array_clear (&input_substeps);
  dang_util_array_clear (&output_substeps);
  dang_util_array_clear (&value_data);
}


/* === SIMPLE_C_INVOKE === */
typedef enum
{
  PARAM_SOURCE_TYPE_NULL,
  PARAM_SOURCE_TYPE_STACK,
  PARAM_SOURCE_TYPE_POINTER,
  PARAM_SOURCE_TYPE_GLOBAL,
  PARAM_SOURCE_TYPE_LITERAL,
  PARAM_SOURCE_TYPE_ZERO
} ParamSourceType;

typedef struct _ParamSourceInfo ParamSourceInfo;
struct _ParamSourceInfo
{
  ParamSourceType type;
  union {
    unsigned stack;
    struct { unsigned ptr, offset; } pointer;
    struct { DangNamespace *ns; unsigned offset; } global;
    struct { unsigned offset; DangValueType *type; } literal;
    struct { unsigned tmp_offset; unsigned size; } zero;
  } info;
};

typedef struct _CompiledSimpleCInvocation CompiledSimpleCInvocation;
struct _CompiledSimpleCInvocation
{
  unsigned n_param_source_infos;
  unsigned tmp_alloc;
  DangSimpleCFunc func;
  void *func_data;
  DangFunction *function;
  unsigned step_size;

  /* ParamSourceInfos follow */
};

static inline void
run_compiled (CompiledSimpleCInvocation *csi,
              DangThreadStackFrame *stack_frame,
              DangThread           *thread,
              void                 *slab)
{
  ParamSourceInfo *psi = (ParamSourceInfo *) (csi+1);
  void **params = slab;
  unsigned i;
  void *ptr;
  DangError *error = NULL;
  for (i = 0; i < csi->n_param_source_infos; i++)
    switch (psi[i].type)
      {
      case PARAM_SOURCE_TYPE_NULL:
        params[i] = NULL;
        break;
      case PARAM_SOURCE_TYPE_STACK:
        params[i] = (char*)stack_frame + psi[i].info.stack;
        break;
      case PARAM_SOURCE_TYPE_POINTER:
        ptr = * (void **) ((char*)stack_frame + psi[i].info.pointer.ptr);
        if (ptr == NULL)
          {
            dang_thread_throw_null_pointer_exception (thread);
            return;
          }
        params[i] = (char*) ptr + psi[i].info.pointer.offset;
        break;
      case PARAM_SOURCE_TYPE_GLOBAL:
        params[i] = psi[i].info.global.ns->global_data + psi[i].info.global.offset;
        break;
      case PARAM_SOURCE_TYPE_LITERAL:
        params[i] = (char*)csi + psi[i].info.literal.offset;
        break;
      case PARAM_SOURCE_TYPE_ZERO:
        memset ((char*)slab + psi[i].info.zero.tmp_offset, 0, psi[i].info.zero.size);
        break;
      }
  if (!csi->func (params + 1, params[0], csi->func_data, &error))
    {
      dang_thread_throw (thread, dang_value_type_error (), &error);
      dang_error_unref (error);
      return;
    }
  dang_thread_stack_frame_advance_ip (stack_frame, csi->step_size);
}

static void
step__run_simple_c__malloc   (void                 *step_data,
                              DangThreadStackFrame *stack_frame,
                              DangThread           *thread)
{
  CompiledSimpleCInvocation *csi = step_data;
  void *slab = dang_malloc (csi->tmp_alloc);
  run_compiled (csi, stack_frame, thread, slab);
  dang_free (slab);
}

static void
step__run_simple_c__alloca (void                 *step_data,
                              DangThreadStackFrame *stack_frame,
                              DangThread           *thread)
{
  CompiledSimpleCInvocation *csi = step_data;
  void *slab = alloca (csi->tmp_alloc);
  run_compiled (csi, stack_frame, thread, slab);
}

static void
init_param_source_info (ParamSourceInfo *psi,
                        DangInsnValue *value,
                        DangInsnPackContext *context,
                        DangArray *literal_data,
                        dang_boolean *needs_destruct_inout)
{
  DangValueType *type = value->type;
  unsigned old_len;
  /* TODO: use ZERO for all output params */
  switch (value->location)
    {
    case DANG_INSN_LOCATION_STACK:
      psi->type = PARAM_SOURCE_TYPE_STACK;
      psi->info.stack = context->vars[value->var].offset;
      break;
    case DANG_INSN_LOCATION_POINTER:
      psi->type = PARAM_SOURCE_TYPE_POINTER;
      psi->info.pointer.ptr = context->vars[value->var].offset;
      psi->info.pointer.offset = value->offset;
      break;
    case DANG_INSN_LOCATION_GLOBAL:
      psi->type = PARAM_SOURCE_TYPE_GLOBAL;
      psi->info.global.ns = value->ns;
      psi->info.global.offset = value->offset;
      break;
    case DANG_INSN_LOCATION_LITERAL:
      /* Align literal data if needed */
      dang_util_array_set_size0 (literal_data,
                            DANG_ALIGN (literal_data->len, type->alignof_instance));

      psi->type = PARAM_SOURCE_TYPE_LITERAL;
      psi->info.literal.type = type;
      psi->info.literal.offset = literal_data->len;

      /* append space */
      old_len = literal_data->len;
      dang_util_array_set_size (literal_data, literal_data->len + type->sizeof_instance);

      /* init_assign */
      if (type->init_assign)
        type->init_assign (type, (char*)literal_data->data + old_len, value->value);
      else
        memcpy ((char*)literal_data->data + old_len, value->value, type->sizeof_instance);

      if (type->destruct != NULL)
        *needs_destruct_inout = TRUE;
      break;
    default:
      dang_assert_not_reached ();
    }
}

static void
compiled_simple_c_destruct_step_data (void *data)
{
  CompiledSimpleCInvocation *csi = data;
  ParamSourceInfo *psi = (ParamSourceInfo *) (csi+1);
  unsigned i;
  for (i = 0; i < csi->n_param_source_infos; i++)
    if (psi[i].type == PARAM_SOURCE_TYPE_LITERAL
     && psi[i].info.literal.type->destruct != NULL)
      psi[i].info.literal.type->destruct (psi[i].info.literal.type,
                                          (char*)data + psi[i].info.literal.offset);

}

static void
pack__run_simple_c (DangInsn *insn,
                    DangInsnPackContext *context)
{
  /* We must form a plan of action for each argument + return-value.
   * - if there is a void return-value or parameter,
   *   we will have to allocate temp space for it.
   * - pointer, global, stack can be handled simply
   * - for literal, we will have to allocate the literal data as
   *   part of the step data.
   */
  DangFunction *function = insn->run_simple_c.func;
  DangInsnValue *args = insn->run_simple_c.args;
  DangSignature *sig = function->base.sig;
  unsigned n_params = sig->n_params;
  DangArray literal_data;
  CompiledSimpleCInvocation *csi;
  ParamSourceInfo *psi;
  unsigned tmp_size;
  unsigned orig_step_size;              /* step size w/o literals */
  dang_boolean needs_destruct = FALSE;
  unsigned i;
  unsigned offset;

  DANG_UTIL_ARRAY_INIT (&literal_data, char);
  tmp_size = (n_params+1) * sizeof (void *);
  orig_step_size = sizeof (CompiledSimpleCInvocation)
                 + (sig->n_params + 1) * sizeof (ParamSourceInfo);
  dang_util_array_set_size (&literal_data, orig_step_size);
  csi = dang_alloca (orig_step_size);
  csi->n_param_source_infos = n_params + 1;
  psi = (ParamSourceInfo *) (csi + 1);
  if (sig->return_type == NULL || sig->return_type == dang_value_type_void ())
    {
      psi[0].type = PARAM_SOURCE_TYPE_NULL;
      offset = 0;
    }
  else
    {
      init_param_source_info (psi + 0, args + 0, context, &literal_data, &needs_destruct);
      offset = 1;
    }
  for (i = 0; i < n_params; i++)
    {
      init_param_source_info (psi + 1 + i, args + i + offset, context, &literal_data, &needs_destruct);
    }
  csi->tmp_alloc = tmp_size;
  if (!function->base.is_owned)
    {
      dang_function_ref (function);
      dang_insn_pack_context_add_destroy (context, (DangInsnDestroyNotify) dang_function_unref,
                                          function, NULL);
    }
  csi->function = function;
  csi->func = function->simple_c.func;
  csi->func_data = function->simple_c.func_data;
  csi->step_size = literal_data.len;

  memcpy (literal_data.data, csi, orig_step_size);

  DangStepRun run_func;
  run_func = (tmp_size > 2048) ? step__run_simple_c__malloc : step__run_simple_c__alloca;

  dang_insn_pack_context_append (context, run_func, literal_data.len, literal_data.data,
                                 needs_destruct ? compiled_simple_c_destruct_step_data : NULL);
  dang_free (literal_data.data);
}

/* === CREATE_CLOSURE === */
typedef struct _CCCInfo CCCInfo;
struct _CCCInfo
{
  unsigned output_offset;               /* where the new function goes */
  DangClosureFactory *factory;
  dang_boolean is_literal;
  union {
    DangFunction *literal;
    unsigned function_offset;
  } underlying;
  unsigned input_offsets[1];            /* more may follow */
};

#define GET_CCC_INFO_SIZE(n_inputs)   \
  (sizeof(CCCInfo) + ((n_inputs)-1) * sizeof(unsigned))


static void
step__create_closure (void                 *step_data,
                      DangThreadStackFrame *stack_frame,
                      DangThread           *thread)
{
  CCCInfo *info = step_data;
  unsigned input_count = dang_closure_factory_get_n_inputs (info->factory);
  void **inputs = dang_newa (void *, input_count);
  unsigned i;
  DangFunction *underlying;
  DangFunction *closure;
  for (i = 0; i < input_count; i++)
    inputs[i] = (char*)stack_frame + info->input_offsets[i];

  if (info->is_literal)
    underlying = info->underlying.literal;
  else
    {
      underlying = * (DangFunction **) ((char*) stack_frame + info->underlying.function_offset);
      if (underlying == NULL)
        {
          dang_thread_throw_null_pointer_exception (thread);
          return;
        }
    }
  if (underlying == NULL)
    {
      dang_warning ("got NULL ptr for function in create_closure (is_literal=%u)",
                    info->is_literal);
      dang_thread_throw_null_pointer_exception (thread);
      return;
    }

  closure = dang_function_new_closure (info->factory, underlying, inputs);
  * (DangFunction **) ((char*)stack_frame + info->output_offset) = closure;

  dang_thread_stack_frame_advance_ip (stack_frame, GET_CCC_INFO_SIZE (input_count));
}

static void
ccc_info_destruct (void *data)
{
  CCCInfo *info = data;
  dang_closure_factory_unref (info->factory);
  if (info->is_literal)
    dang_function_unref (info->underlying.literal);
}

static void
pack__create_closure (DangInsn *insn,
                      DangInsnPackContext *context)
{
  unsigned i;
  unsigned info_size;
  CCCInfo *info;
  DangClosureFactory *factory = insn->create_closure.factory;
  unsigned n_inputs = dang_closure_factory_get_n_inputs (factory);

  info_size = GET_CCC_INFO_SIZE (n_inputs);
  info = dang_malloc (info_size);
  info->factory = dang_closure_factory_ref (factory);
  info->output_offset = context->vars[insn->create_closure.target].offset;
  for (i = 0; i < n_inputs; i++)
    info->input_offsets[i] = context->vars[insn->create_closure.input_vars[i]].offset;
  if (insn->create_closure.is_literal)
    {
      DangFunction *fct;
      info->is_literal = TRUE;
      info->underlying.literal = insn->create_closure.underlying.literal;
      dang_function_ref (info->underlying.literal);
      fct = info->underlying.literal;
      dang_assert (fct);
    }
  else
    {
      info->is_literal = FALSE;
      info->underlying.function_offset = context->vars[insn->create_closure.underlying.function_var].offset;
    }

  dang_insn_pack_context_append (context, step__create_closure, info_size, info, ccc_info_destruct);
  dang_free (info);
}

/* === PUSH_CATCH_GUARD === */
static void
step__push_catch_guard (void *step_data,
                           DangThreadStackFrame *stack_frame,
                           DangThread           *thread)
{
  unsigned cb_index = *(unsigned*) step_data;
  DangFunctionStackInfo *stack_info = stack_frame->function->base.stack_info;
  dang_assert (stack_info);
  dang_assert (cb_index < stack_info->n_catch_blocks);
  dang_thread_push_catch_guard (thread,
                                stack_frame,
                                stack_info->catch_blocks + cb_index);
  dang_thread_stack_frame_advance_ip (stack_frame, sizeof(unsigned));
}

static void
pack__push_catch_guard (DangInsn *insn,
                        DangInsnPackContext *context)
{
  dang_insn_pack_context_append (context,
                                 step__push_catch_guard,
                                 sizeof(unsigned),
                                 &insn->push_catch_guard.catch_block_index,
                                 NULL);
}

/* === POP_CATCH_GUARD === */

static void
step__pop_catch_guard  (void *step_data,
                           DangThreadStackFrame *stack_frame,
                           DangThread           *thread)
{
  DANG_UNUSED (step_data);
  dang_thread_pop_catch_guard (thread);
  dang_thread_stack_frame_advance_ip (stack_frame, 0);
}

static void
pack__pop_catch_guard (DangInsn *insn,
                       DangInsnPackContext *context)
{
  DANG_UNUSED (insn);
  dang_insn_pack_context_append (context,
                                 step__pop_catch_guard,
                                 0, NULL, NULL);
}

/* === RETURN === */
static void
step__return    (void                 *step_data,
                 DangThreadStackFrame *stack_frame,
                 DangThread           *thread)
{
  DANG_UNUSED (step_data);
  thread->stack_frame = stack_frame->caller;
  if (thread->stack_frame == NULL)
    {
      thread->status = DANG_THREAD_STATUS_DONE;
      dang_thread_unref (thread);
    }
}

static void
pack__return (DangInsn *insn,
              DangInsnPackContext *context)
{
  DANG_UNUSED (insn);
  dang_insn_pack_context_append (context, step__return, 0, NULL, NULL);
}


/* === INDEX === */
/* For setting back an element of the array, like a[1] = 3; */
typedef struct _IndexStepData IndexStepData;
struct _IndexStepData
{
  DangValueType *element_type;
  PackedValue element;
  PackedValue container;
  DangValueIndexInfo *index_info;
  PackedValue indices[1];
};

static void
step__index_lvalue (void *sd,
                            DangThreadStackFrame *stack_frame,
                            DangThread           *thread)
{
  IndexStepData *step_data = sd;
  DangValueIndexInfo *ii = step_data->index_info;
  unsigned i;
  const void **indices = dang_newa (const void *, ii->n_indices);
  DangError *error = NULL;
  void *container = peek_packed_value (&step_data->container, stack_frame);
  void *elt;
  if (container == NULL)
    goto null_ptr_exception;
  for (i = 0; i < ii->n_indices; i++)
    if ((indices[i] = peek_packed_value (step_data->indices + i,
                                         stack_frame)) == NULL)
      goto null_ptr_exception;
  elt = peek_packed_value (&step_data->element, stack_frame);
  if (elt == NULL)
    goto null_ptr_exception;
  if (!step_data->index_info->set (step_data->index_info, container,
                                   indices, elt, TRUE,
                                   &error))
    {
      dang_thread_throw_error (thread, error);
      dang_error_unref (error);
      return;
    }

  dang_thread_stack_frame_advance_ip (stack_frame,
                                      sizeof (IndexStepData)
                                      + (ii->n_indices-1) * sizeof(PackedValue));
  return;

null_ptr_exception:
  dang_thread_throw_null_pointer_exception (thread);
  return;
}
static void
step__index_rvalue (void *sd,
                    DangThreadStackFrame *stack_frame,
                    DangThread           *thread)
{
  IndexStepData *step_data = sd;
  DangValueIndexInfo *ii = step_data->index_info;
  unsigned i;
  const void **indices = dang_newa (const void *, ii->n_indices);
  DangError *error = NULL;
  void *container = peek_packed_value (&step_data->container, stack_frame);
  void *elt;
  if (container == NULL)
    goto null_ptr_exception;
  for (i = 0; i < ii->n_indices; i++)
    if ((indices[i] = peek_packed_value (step_data->indices + i,
                                         stack_frame)) == NULL)
      goto null_ptr_exception;
  elt = peek_packed_value (&step_data->element, stack_frame);
  if (elt == NULL)
    goto null_ptr_exception;
  if (!step_data->index_info->get (step_data->index_info, container,
                                   indices,
                                   elt, FALSE,
                                   &error))
    {
      dang_thread_throw_error (thread, error);
      dang_error_unref (error);
      return;
    }

  dang_thread_stack_frame_advance_ip (stack_frame,
                                      sizeof (IndexStepData)
                                      + (ii->n_indices - 1) * sizeof(PackedValue));
  return;

null_ptr_exception:
  dang_thread_throw_null_pointer_exception (thread);
}

static void
pack__index (DangInsn *insn,
             DangInsnPackContext *context)
{
  DangValueIndexInfo *ii = insn->index.index_info;
  IndexStepData *rv;
  unsigned sd_size, i;
  unsigned rank = ii->n_indices;
  DangStepRun stepfunc;
  dang_assert (insn->index.container.type == insn->index.index_info->owner);
  dang_assert (rank >= 1);
  sd_size = sizeof (IndexStepData) + (rank - 1) * sizeof(PackedValue);
  rv = dang_alloca (sd_size);
  rv->element_type = ii->element_type;
  pack_value_location (context, &insn->index.element, &rv->element);
  rv->index_info = insn->index.index_info;
  for (i = 0; i < rank; i++)
    {
      pack_value_location (context, &insn->index.indices[i], 
                           rv->indices + i);
    }
  pack_value_location (context, &insn->index.container,
                       &rv->container);
  if (insn->index.is_set)
    stepfunc = step__index_lvalue;
  else
    stepfunc = step__index_rvalue;
  dang_insn_pack_context_append (context, stepfunc, sd_size, rv, NULL);
}

static void
step__new_tensor (void *sd,
                  DangThreadStackFrame *stack_frame,
                  DangThread           *thread)
{
  unsigned *data = sd;
  unsigned rank = data[0];
  unsigned total_size = data[1];
  unsigned offset = data[2];
  unsigned *sizes = data + 3;
  DangTensor **p_tensor = (DangTensor **) ((char*)stack_frame + offset);
  DangTensor *tensor = dang_malloc (DANG_TENSOR_SIZEOF (rank));
  DANG_UNUSED (thread);
  tensor->ref_count = 1;
  memcpy (tensor->sizes, sizes, sizeof (unsigned) * rank);
  tensor->data = dang_malloc0 (total_size);
  *p_tensor = tensor;
  dang_thread_stack_frame_advance_ip (stack_frame,
                                      sizeof(unsigned) * (3 + rank));
}

static void
pack__new_tensor (DangInsn *insn,
                  DangInsnPackContext *context)
{
  unsigned *rv;
  unsigned sd_size;
  unsigned rank = insn->new_tensor.rank;
  DangStepRun stepfunc;
  dang_assert (rank >= 1);
  sd_size = sizeof (unsigned) * (3 + rank);
  rv = dang_alloca (sd_size);
  rv[0] = rank;
  rv[1] = insn->new_tensor.total_size;
  rv[2] = context->vars[insn->new_tensor.target].offset;
  memcpy (rv + 3, insn->new_tensor.dims, sizeof (unsigned) * rank);
  stepfunc = step__new_tensor;
  dang_insn_pack_context_append (context, stepfunc, sd_size, rv, NULL);
}

typedef void (*PackFunc) (DangInsn *insn,
                          DangInsnPackContext *context);


/* NOTE: must match DangInsnType exactly */
static PackFunc pack_funcs[13] = 
{
  pack__init,
  pack__destruct,
  pack__assign,
  pack__jump,
  pack__jump_conditional,
  pack__function_call,
  pack__run_simple_c,
  pack__push_catch_guard,
  pack__pop_catch_guard,
  pack__return,
  pack__index,
  pack__create_closure,
  pack__new_tensor
};

void
dang_insn_pack (DangInsn             *insn,
                DangInsnPackContext  *context)
{
  dang_assert (insn->type < DANG_N_ELEMENTS (pack_funcs));
  pack_funcs[insn->type] (insn, context);
}
