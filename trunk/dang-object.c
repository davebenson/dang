#include <string.h>
#include "dang.h"
#include "magic.h"
#include "config.h"

#define DEBUG_OBJECT_REF_COUNT  0

#if DEBUG_OBJECT_REF_COUNT
# define DEBUG_OBJECT_REF_COUNT_MSG(args) dang_warning args
#else
# define DEBUG_OBJECT_REF_COUNT_MSG(args) 
#endif


typedef struct _NonMemcpyMember NonMemcpyMember;
struct _NonMemcpyMember
{
  DangValueType *type;
  unsigned offset;
};

static void
init_assign__object (DangValueType *type,
                     void          *dst,
                     const void    *src)
{
  DangObject *p = * (void **) src;
  DANG_UNUSED (type);
  if (p != NULL)
    dang_object_ref_inlined (p);
  * (void **) dst = p;
}

static void
assign__object (DangValueType *type,
                void          *dst,
                const void    *src)
{
  DangObject *s = * (void **) src;
  DangObject *d = * (void **) dst;
  DANG_UNUSED (type);
  if (s)
    dang_object_ref_inlined (s);
  if (d)
    dang_object_unref_inlined (d);
  * (void**) dst = s;
}

static void
destruct__object (DangValueType *type,
                  void          *value)
{
  DangObject *o = * (void **) value;
  DANG_UNUSED (type);
  if (o)
    dang_object_unref_inlined (o);
}

static DANG_VALUE_COMPARE_FUNC_DECLARE(compare__object)
{
  DangObject *oa = *(DangObject**)a;
  DangObject *ob = *(DangObject**)b;
  DANG_UNUSED (type);
  return oa < ob ? -1 : oa > ob ? 1 : 0;
}
static DANG_VALUE_HASH_FUNC_DECLARE(hash__object)
{
  DangObject *oa = *(DangObject**)a;
  DANG_UNUSED (type);
  return (uint32_t) (size_t) oa;
}
static DANG_VALUE_EQUAL_FUNC_DECLARE(equal__object)
{
  DangObject *oa = *(DangObject**)a;
  DangObject *ob = *(DangObject**)b;
  DANG_UNUSED (type);
  return oa == ob;
}
static char *
to_string__object (DangValueType *type,
                   const void    *value)
{
  DangObject *o = * (void **) value;
  if (o == NULL)
    return dang_strdup_printf ("%s[%p]: NULL", type->full_name, type);
  else
    {
      DangObjectClass *class = o->the_class;
      DangValueType *real_type = class->type;
      if (type != real_type)
        return real_type->to_string (real_type, value);
      else
        return dang_strdup_printf ("%s[%p]: %p", type->full_name, type, o);
    }
}

static DangValueTypeObject the_type =
{
  {
    DANG_VALUE_TYPE_MAGIC,
    1,                        /* ref-count */
    "dang.Object",
    sizeof (void *),
    DANG_ALIGNOF_POINTER,
    init_assign__object,
    assign__object,
    destruct__object,
    compare__object,
    hash__object,
    equal__object,
    to_string__object,
    NULL, NULL,               /* no casting */
    DANG_VALUE_INTERNALS_INIT
  },
  sizeof (DangObjectClass),
  sizeof (DangObject),
  NULL,         /* class */
  NULL,         /* prototype instance */
  0,            /* class_alloced */
  0,            /* instance_alloced */
  FALSE,        /* subclassed */
  FALSE,        /* instantiated */
  FALSE,        /* compiled_virtuals */
  NULL, NULL, NULL, NULL,       /* type-tree pointers */
  DANG_UTIL_ARRAY_STATIC_INIT (NonMemcpyMember),
  DANG_UTIL_ARRAY_STATIC_INIT (unsigned)
};
DangValueType *dang_value_type_object (void)
{
  if (the_type.the_class == NULL)
    {
      DangObject *obj;
      the_type.class_alloced = sizeof (DangObjectClass);
      the_type.the_class = dang_new (DangObjectClass, 1);
      ((DangObjectClass*)the_type.the_class)->type = &the_type.base_type;
      the_type.instance_alloced = sizeof (DangObject);
      the_type.prototype_instance = obj = dang_new (DangObject, 1);
      obj->the_class = the_type.the_class;
      obj->ref_count = 1;
      obj->weak_ref = NULL;
    }
  return &the_type.base_type;
}
dang_boolean dang_value_type_is_object (DangValueType *type)
{
  return type->init_assign == init_assign__object;
}

/* Create a new subtype of a DangObject's type. */
DangValueType *
dang_object_type_subclass (DangValueType *parent_type,
                           const char    *full_name)
{
  DangValueTypeObject *rv = dang_new0 (DangValueTypeObject, 1);
  DangValueTypeObject *parent_otype = (DangValueTypeObject *) parent_type;
  DangValueTypeObject *c;
  unsigned i;
  dang_assert (dang_value_type_is_object (parent_type));

  rv->base_type.magic = DANG_VALUE_TYPE_MAGIC;
  rv->base_type.ref_count = 1;
  rv->base_type.full_name = dang_strdup (full_name);
  rv->base_type.sizeof_instance = sizeof (void *);
  rv->base_type.alignof_instance = DANG_ALIGNOF_POINTER;
  rv->base_type.init_assign = init_assign__object;
  rv->base_type.assign = assign__object;
  rv->base_type.destruct = destruct__object;
  rv->base_type.compare = parent_type->compare;
  rv->base_type.hash = parent_type->hash;
  rv->base_type.equal = parent_type->equal;
  rv->base_type.to_string = parent_type->to_string;
  rv->base_type.cast_func_name = parent_type->cast_func_name;
  rv->base_type.internals.parent = parent_type;

  /* add to type tree */
  if (parent_otype->first_child == NULL)
    parent_otype->first_child = rv;
  else
    parent_otype->last_child->next_sibling = rv;
  rv->prev_sibling = parent_otype->last_child;
  parent_otype->last_child = rv;

  //memcpy (rv, parent_type, sizeof (DangValueTypeObject));
  parent_otype->subclassed = TRUE;
  rv->subclassed = FALSE;
  rv->instantiated = FALSE;
  rv->compiled_virtuals = FALSE;

  /* Create new class and prototype instance */
  rv->class_size = parent_otype->class_size;
  rv->instance_size = parent_otype->instance_size;
  rv->class_alloced = parent_otype->class_alloced;
  rv->instance_alloced = parent_otype->instance_alloced;
  DANG_UTIL_ARRAY_INIT (&rv->mutable_fct_offsets, unsigned);
  DANG_UTIL_ARRAY_INIT (&rv->non_memcpy_members, NonMemcpyMember);
  rv->the_class = dang_malloc (rv->class_alloced);
  rv->prototype_instance = dang_malloc (rv->instance_alloced);
  ((DangObjectClass *) rv->the_class)->type = &rv->base_type;
  for (i = sizeof (DangObjectClass);
       i < rv->class_size;
       i += sizeof (DangDestroyNotify))
    {
      DangFunction *f = * (DangFunction **) ((char*)parent_otype->the_class + i);
      if (f)
        dang_function_ref (f);
      * (DangFunction **) ((char*)rv->the_class + i) = f;
    }

  char *src_proto;
  char *dst_proto;
  DangObject *proto_obj;
  src_proto = (char*) parent_otype->prototype_instance;
  dst_proto = (char*) rv->prototype_instance;
  memcpy (dst_proto, src_proto, rv->instance_size);
  proto_obj = (DangObject *) dst_proto;
  proto_obj->ref_count = 1;
  proto_obj->the_class = rv->the_class;
  for (c = parent_otype; c; c = (DangValueTypeObject*)(c->base_type.internals.parent))
    {
      for (i = 0; i < c->non_memcpy_members.len; i++)
        {
          NonMemcpyMember *nmm = ((NonMemcpyMember*)c->non_memcpy_members.data) + i;
          nmm->type->init_assign (nmm->type,
                                 dst_proto + nmm->offset,
                                 src_proto + nmm->offset);
        }
    }

  return &rv->base_type;
}

static inline void
ensure_class_large_enough (DangValueTypeObject *otype)
{
  if (otype->class_size > otype->class_alloced)
    {
      otype->class_alloced *= 2;
      while (otype->class_size > otype->class_alloced)
        otype->class_alloced *= 2;
      otype->the_class = dang_realloc (otype->the_class, otype->class_alloced);
    }
  ((DangObject*)otype->prototype_instance)->the_class = otype->the_class;
}

static inline void
ensure_instance_large_enough (DangValueTypeObject *otype)
{
  if (otype->instance_size > otype->instance_alloced)
    {
      otype->instance_alloced *= 2;
      while (otype->instance_size > otype->instance_alloced)
        otype->instance_alloced *= 2;
      otype->prototype_instance = dang_realloc (otype->prototype_instance, otype->instance_alloced);
    }
}

dang_boolean
dang_object_add_method  (DangValueType  *object_type,
                         const char     *name,
                         DangMethodFlags flags,
                         DangFunction   *func,
                         DangError     **error)
{
  /* Check conditions */
  DangValueType *method_type;
  DangValueElement *elt;
  unsigned method_index;
  DangValueTypeObject *otype = (DangValueTypeObject *) object_type;
  DangError *e = NULL;
  dang_assert (dang_value_type_is_object (object_type));
  dang_assert (!otype->subclassed);
  dang_assert (!otype->instantiated);

  /* may return an error, or not */
  if (dang_value_type_find_method_by_sig (object_type, name, flags, func->base.sig,
                                          &method_type, &elt, &method_index, &e))
    {
      /* Overload, unless we already overloaded it. */
      DangValueMethod *method = ((DangValueMethod*)elt->info.methods.data) + method_index;
      DangValueTypeObject *parent_otype = (DangValueTypeObject*) object_type->internals.parent;
      if (method->flags & DANG_METHOD_FINAL)
        {
          dang_set_error (error, "cannot overload final method %s",
                          name);
          return FALSE;
        }
      if (flags != (method->flags & (~(DANG_METHOD_ABSTRACT))))
        {
          dang_set_error (error, "method %s differs in flags 0x%x v 0x%x",
                          name, flags, method->flags);
          return FALSE;
        }
      if (flags & DANG_METHOD_ABSTRACT)
        {
          dang_set_error (error, "abstract method given a body");
          return FALSE;
        }
      if (method_type == object_type)
        {
          dang_set_error (error, "function %s already defined in %s",
                          name, object_type->full_name);
          return FALSE;
        }

      /* NOTE: assumes that all are methods are 'simple' */
      unsigned offset;
      offset = method->offset;
      DangFunction **pfunc, **parent_pfunc;
      if (flags & DANG_METHOD_MUTABLE)
        {
          pfunc = (DangFunction**) ((char*)otype->prototype_instance + offset);
          parent_pfunc = (DangFunction**) ((char*)parent_otype->prototype_instance + offset);
        }
      else
        {
          pfunc = (DangFunction**) ((char*)otype->the_class + offset);
          parent_pfunc = (DangFunction**) ((char*)parent_otype->the_class + offset);
        }

      if (*pfunc != *parent_pfunc)
        {
          dang_set_error (error, "already overloaded function %s()", name);
          return FALSE;
        }
      if (*pfunc != NULL)
        dang_function_unref (*pfunc);
      *pfunc = dang_function_attach_ref (func);
      return TRUE;
    }
  else
    {
      /* If it is a member, return the error.
         Otherwise, add the method. */
      if (e)
        {
          *error = e;
          return FALSE;
        }
    }
  if ((flags & DANG_METHOD_ABSTRACT) != 0)
    {
      dang_set_error (error, "cannot add concrete abstract method %s to %s",
                      name, object_type->full_name);
      return FALSE;
    }
  if ((flags & DANG_METHOD_STATIC) == 0)
    {
      DangSignature *sig = func->base.sig;
      if (sig->n_params < 1 || sig->params[0].type != object_type
          || strcmp (sig->params[0].name, "this") != 0)
        {
          dang_set_error (error, "cannot add non-static method %s to %s whose first argument is not 'this'",
                          name, object_type->full_name);
          return FALSE;
        }
    }
  if ((flags & DANG_METHOD_FINAL) == 0)
    {
      /* must be virtual or mutable. */
      if ((flags & DANG_METHOD_MUTABLE) == 0)
        {
          unsigned offset = otype->class_size;
          otype->class_size += sizeof (DangDestroyNotify);
          dang_value_type_add_simple_virtual_method
                      (object_type, name, flags,
                       func->base.sig, offset);
          ensure_class_large_enough (otype);
          * (DangFunction **) ((char*)otype->the_class + offset) = func;
        }
      else
        {
          unsigned offset;
          NonMemcpyMember nmm;
          otype->instance_size = DANG_ALIGN (otype->instance_size,
                                             DANG_ALIGNOF_POINTER);
          offset = otype->instance_size; 
          otype->instance_size += sizeof (DangDestroyNotify);

          ensure_instance_large_enough (otype);

          dang_value_type_add_simple_mutable_method
                      (object_type, name, flags,
                       func->base.sig, offset);
          * (DangFunction **) ((char*)otype->prototype_instance + offset) = func;
          nmm.offset = offset;
          nmm.type = dang_value_type_function (func->base.sig);
          dang_util_array_append (&otype->non_memcpy_members, 1, &nmm);
        }
      dang_function_attach_ref (func);
    }
  else
    {
      dang_value_type_add_constant_method (object_type, name, flags, func);
    }
  return TRUE;
}

void
dang_object_note_method_stubs (DangValueType *type,
                               DangCompileContext *cc)
{
  unsigned i;
  DangValueTypeObject *otype = (DangValueTypeObject *) type;
  char *class = otype->the_class;
  char *instance = otype->prototype_instance;

  dang_assert (dang_value_type_is_object (type));

  if (otype->compiled_virtuals)
    return;
  otype->compiled_virtuals = 1;
  
  /* All pointers in the class are Functions, i guess, so we just
     do a quick brute-force loop over them. */
  for (i = sizeof (DangObjectClass);
       i < otype->class_size;
       i += sizeof (DangDestroyNotify))
    {
      DangFunction *fct = *(DangFunction**)(class + i);
      if (fct && dang_function_needs_registration (fct))
        dang_compile_context_register (cc, fct);
    }

  /* Walk up the type tree, adding mutable methods from the instance */
  do
    {
      for (i = 0; i < otype->mutable_fct_offsets.len; i++)
        {
          unsigned offset = ((unsigned*)otype->mutable_fct_offsets.data)[i];
          DangFunction *fct = *(DangFunction**)(instance + offset);
          if (fct && dang_function_needs_registration (fct))
            dang_compile_context_register (cc, fct);
        }
      type = type->internals.parent;
      otype = (DangValueTypeObject*) type;
    }
  while (type);
}

dang_boolean
dang_object_add_abstract_method (DangValueType  *object_type,
                                 const char     *name,
                                 DangMethodFlags flags,
                                 DangSignature  *sig,
                                 DangError     **error)
{
  DangValueTypeObject *otype = (DangValueTypeObject *)object_type;
  DangValueType *method_type;
  DangValueElement *elt;
  unsigned method_index;
  DangError *e = NULL;
  if (dang_value_type_find_method_by_sig (object_type, name,
                                   flags, sig,
                                   &method_type, &elt, &method_index,
                                   &e))
    {
      /* Abstract method already exists? */
      dang_set_error (error, "method %s already exists in %s", name, object_type->full_name);
      return FALSE;
    }
  else if (e)
    {
      *error = e;
      return FALSE;
    }

  if ((flags & DANG_METHOD_ABSTRACT) == 0)
    {
      dang_set_error (error, "cannot add non-abstract method %s to %s using dang_value_object_add_abstract_method",
                      name, object_type->full_name);
      return FALSE;
    }
  if ((flags & DANG_METHOD_FINAL) != 0)
    {
      dang_set_error (error, "abstract and final are not compatible (method %s of %s)",
                      name, object_type->full_name);
      return FALSE;
    }


  /* TODO: this code is mighty similar to some from add_method */
  if ((flags & DANG_METHOD_MUTABLE) == 0)
    {
      unsigned offset = otype->class_size;
      otype->class_size += sizeof (DangDestroyNotify);
      dang_value_type_add_simple_virtual_method
                  (object_type, name, flags,
                   sig, offset);
      ensure_class_large_enough (otype);
      * (DangFunction **) ((char*)otype->the_class + offset) = NULL;
    }
  else
    {
      unsigned offset;
      NonMemcpyMember nmm;
      otype->instance_size = DANG_ALIGN (otype->instance_size,
                                         DANG_ALIGNOF_POINTER);
      offset = otype->instance_size; 
      otype->instance_size += sizeof (DangDestroyNotify);

      ensure_instance_large_enough (otype);

      dang_value_type_add_simple_mutable_method
                  (object_type, name, flags,
                   sig, offset);
      * (DangFunction **) ((char*)otype->prototype_instance + offset) = NULL;
      nmm.offset = offset;
      nmm.type = dang_value_type_function (sig);
      dang_util_array_append (&otype->non_memcpy_members, 1, &nmm);
    }
  return TRUE;
}

static void
step__new (void                 *step_data,
           DangThreadStackFrame *stack_frame,
           DangThread           *thread)
{
  DangFunction *fct = (DangFunction*)((char*)stack_frame->ip - offsetof (DangFunctionNewObject, step));
  void **p_rv;
  dang_assert (fct->type == DANG_FUNCTION_TYPE_NEW_OBJECT);

  DANG_UNUSED (step_data);
  DANG_UNUSED (thread);

  p_rv = (void **) (stack_frame + 1);
  *p_rv = dang_object_new (fct->new_object.object_type);


  /* tail call! */
  stack_frame->function = fct->new_object.constructor;
  stack_frame->ip = stack_frame->function->base.steps;
}

dang_boolean
dang_object_add_constructor (DangValueType  *object_type,
                             const char     *name,
                             DangFunction   *func,
                             DangError     **error)
{
  DangFunction *new_func;
  DANG_UNUSED (error);

  /* Create a function that does an allocation then just
     "tail-calls" 'func'. */
  new_func = dang_new0 (DangFunction, 1);
  new_func->base.type = DANG_FUNCTION_TYPE_NEW_OBJECT;
  new_func->base.ref_count = 1;
  new_func->base.compile = NULL;
  new_func->base.stack_info = NULL;
  new_func->base.sig = dang_signature_new (object_type,
                                           func->base.sig->n_params - 1,
                                           func->base.sig->params + 1);
  new_func->base.frame_size = func->base.frame_size;
  new_func->base.steps = &new_func->new_object.step;
  new_func->new_object.step.func = step__new;
  new_func->new_object.step._step_data_size = 0;
  new_func->new_object.object_type = object_type;
  new_func->new_object.must_unref_constructor = 0;
  if (!func->base.is_owned)
    {
      dang_function_ref (func);
      new_func->new_object.must_unref_constructor = 1;
    }
  new_func->new_object.constructor = func;

  dang_value_type_add_ctor (object_type, name, new_func);
  dang_function_unref (new_func);
  return TRUE;
}

dang_boolean
dang_object_add_member (DangValueType  *object_type,
                        const char     *name,
                        DangMemberFlags flags,
                        DangValueType  *member_type,
                        const void     *default_value,
                        DangError     **error)
{
  DangValueTypeObject *otype = (DangValueTypeObject *)object_type;
  DangValueElement *elt;
  unsigned offset;
  void *proto_member;
  elt = dang_value_type_lookup_element (object_type, name, TRUE, NULL);
  if (elt != NULL && elt->element_type != DANG_VALUE_ELEMENT_TYPE_MEMBER)
    {
      dang_set_error (error, "cannot add method %s to %s: a method of that name exists",
                      name, object_type->full_name);
      return FALSE;
    }

  offset = DANG_ALIGN (otype->instance_size, member_type->alignof_instance);
  otype->instance_size = offset + member_type->sizeof_instance;
  ensure_instance_large_enough (otype);
  proto_member = (char*)otype->prototype_instance + offset;
  if (default_value)
    {
      if (member_type->init_assign)
        member_type->init_assign (member_type, proto_member, default_value);
      else
        memcpy (proto_member, default_value, member_type->sizeof_instance);
    }
  else
    memset (proto_member, 0, member_type->sizeof_instance);

  if (member_type->init_assign)
    {
      NonMemcpyMember nmm;
      nmm.offset = offset;
      nmm.type = member_type;
      dang_util_array_append (&otype->non_memcpy_members, 1, &nmm);
    }
  dang_value_type_add_simple_member (object_type, name, flags, member_type,
                                     TRUE, offset);
  return TRUE;
}

void *
dang_object_new (DangValueType *type)
{
  DangValueTypeObject *o = (DangValueTypeObject *) type;
  void *rv;
  DangValueTypeObject *c;
  unsigned i;
  dang_assert (dang_value_type_is_object (type));
  rv = dang_memdup (o->prototype_instance, o->instance_size);
  for (c = o; c != NULL; c = (DangValueTypeObject*)(c->base_type.internals.parent))
    {
      NonMemcpyMember *nmm = c->non_memcpy_members.data;
      for (i = 0; i < c->non_memcpy_members.len; i++)
        nmm[i].type->init_assign (nmm[i].type,
                                 (char*)rv + nmm[i].offset,
                                 (char*)c->prototype_instance + nmm[i].offset);
    }
  return rv;
}

/* TODO: one unref can lead to a very long chain of unrefs that
   cause deep recursion.  we should implement at least a manual
   recursion guard.  this also has the advantage that we could
   incrementalize the labor. */
void
dang_object_unref (void *obj)
{
  DangObject *o = obj;
  DangValueTypeObject *c;
  unsigned i;
  dang_assert (o->ref_count > 0);
  dang_assert (dang_value_type_is_object (o->the_class->type));
  DEBUG_OBJECT_REF_COUNT_MSG (("dang_object_unref(%p:%s): %u => %u", o, o->the_class->type->full_name, o->ref_count, o->ref_count-1));
  if (--(o->ref_count) == 0)
    {
      for (c = (DangValueTypeObject *) o->the_class->type;
           c != NULL;
           c = (DangValueTypeObject*)(c->base_type.internals.parent))
        {
          NonMemcpyMember *nmm = c->non_memcpy_members.data;
          for (i = 0; i < c->non_memcpy_members.len; i++)
            nmm[i].type->destruct (nmm[i].type,
                                     (char*)obj + nmm[i].offset);
        }
      dang_free (o);
    }
}

void *
dang_object_ref (void *obj)
{
  DangObject *o = obj;
  dang_assert (dang_value_type_is_object (o->the_class->type));
  dang_assert (o->ref_count > 0);
  DEBUG_OBJECT_REF_COUNT_MSG (("dang_object_ref(%p:%s): %u => %u", o, o->the_class->type->full_name, o->ref_count, o->ref_count+1));
  ++(o->ref_count);
  return o;
}

static void
cleanup_object_type_recursive1 (DangValueTypeObject *o)
{
  char *proto;
  unsigned i;
  DangValueTypeObject *dtype;
  /* clean our children (recursively) */
  for (dtype = o->first_child; dtype != NULL; dtype = dtype->next_sibling)
    cleanup_object_type_recursive1 (dtype);

  /* unref functions in vtable, prototype instance */
  proto = o->prototype_instance;
  for (dtype = o; dtype != NULL; dtype = (DangValueTypeObject*)(dtype->base_type.internals.parent))
    {
      NonMemcpyMember *nmm = dtype->non_memcpy_members.data;
      unsigned *mfo = dtype->mutable_fct_offsets.data;
      for (i = 0; i < dtype->non_memcpy_members.len; i++)
        {
          void *value = proto + nmm[i].offset;
          DangValueType *type = nmm[i].type;
          type->destruct (type, value);
        }
      for (i = 0; i < dtype->mutable_fct_offsets.len; i++)
        {
          DangFunction *f = *(DangFunction**)(proto + mfo[i]);
          dang_function_unref (f);
        }
    }

  /* XXX: this is not going to work if we have more than just DangFunction*'s in the class */
  for (i = sizeof (DangObjectClass); i < o->class_size; i += sizeof (DangFunction*))
    {
      DangFunction *f = *(DangFunction**)((char*)o->the_class + i);
      if (f)
        dang_function_unref (f);
    }

}
static void
free_object_type_recursive2 (DangValueTypeObject *o)
{
  DangValueTypeObject *dtype;
  /* free our children (recursively) */
  for (dtype = o->first_child; dtype != NULL; )
    {
      DangValueTypeObject *next = dtype->next_sibling;
      free_object_type_recursive2 (dtype);
      dtype = next;
    }

  dang_util_array_clear (&o->non_memcpy_members);
  dang_util_array_clear (&o->mutable_fct_offsets);

  dang_value_type_cleanup (&o->base_type);

  dang_free (o->the_class);
  dang_free (o->prototype_instance);
  dang_free (o->base_type.full_name);
  dang_free (o);
}

/* Cleanup that requires all the types to exist.
   Namely, destroying the instances and classes. */
void
_dang_object_cleanup1 (void)
{
  DangValueTypeObject *o;
  for (o = the_type.first_child; o != NULL; o = o->next_sibling)
    cleanup_object_type_recursive1 (o);
}

/* Cleanup that actually frees the types. */
void
_dang_object_cleanup2 (void)
{
  DangValueTypeObject *o;
  for (o = the_type.first_child; o != NULL; )
    {
      DangValueTypeObject *next = o->next_sibling;
      free_object_type_recursive2 (o);
      o = next;
    }
  the_type.first_child = the_type.last_child = NULL;
  dang_free (the_type.the_class);
  the_type.the_class = NULL;
  dang_free (the_type.prototype_instance);
  the_type.prototype_instance = NULL;
}
