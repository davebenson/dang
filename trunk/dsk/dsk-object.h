
struct _DskObjectClass
{
  const char *name;
  DskObjectClass *parent_class;
  size_t sizeof_class;
  size_t sizeof_instance;
  unsigned object_class_magic;
  void (*init) (DskObject *object);
  void (*finalize) (DskObject *object);
};
typedef void (*DskObjectInitFunc) (DskObject *object);
typedef void (*DskObjectFinalizeFunc) (DskObject *object);
#define DSK_OBJECT_CLASS_MAGIC          0x159daf3f
#define DSK_OBJECT_CLASS_DEFINE(name, parent_class, init_func, finalize_func) \
       { #name, (DskObjectClass *) parent_class, \
         sizeof (name ## Class), sizeof (name), \
         DSK_OBJECT_CLASS_MAGIC, \
         (DskObjectInitFunc) init_func, (DskObjectFinalizeFunc) finalize_func }


struct _DskObject
{
  DskObjectClass *object_class;
  unsigned ref_count;
};

                 void      *dsk_object_new   (void *object_class);
DSK_INLINE_FUNCS void       dsk_object_unref (void *object);
DSK_INLINE_FUNCS DskObject *dsk_object_ref   (void *object);
                 dsk_boolean dsk_object_is_a (void *object,
                                              void *isa_class);
                 void       *dsk_object_cast (void *object,
                                              void *isa_class,
                                              const char *filename,
                                              unsigned line);
                 void       *dsk_object_cast_get_class (void *object,
                                              void *isa_class,
                                              const char *filename,
                                              unsigned line);

/* non-inline versions of dsk_object_{ref,unref} */
                 void       dsk_object_unref_f (void *object);
                 void       dsk_object_ref_f (void *object);

#ifdef DSK_DISABLE_CAST_CHECKS
#define DSK_OBJECT_CAST(type, object, isa_class) ((type*)(object))
#define DSK_OBJECT_CAST_GET_CLASS(type, object, isa_class) ((type##Class*)(((DskObject*)(object))->object_class))
#else
#define DSK_OBJECT_CAST(type, object, isa_class) \
  ((type*)dsk_object_cast(object, isa_class, __FILE__, __LINE__))
#define DSK_OBJECT_CAST_GET_CLASS(type, object, isa_class) ((type##Class*)(((DskObject*)(object))->object_class)) \
  ((type##Class*)dsk_object_cast_get_class(object, isa_class, __FILE__, __LINE__))
#endif

#if DSK_CAN_INLINE || DSK_IMPLEMENT_INLINES
DSK_INLINE_FUNCS void      *dsk_object_new   (void *object_class)
{
  DskObjectClass *c = object_class;
  DskObject *rv;
  dsk_assert (c->magic == DSK_OBJECT_CLASS_MAGIC);
  rv = dsk_malloc (c->sizeof_instance);
  rv->object_class = object_class;
  dsk_bzero_pointers (rv + 1, c->pointers_after_base);
  do
    {
      if (c->init_func != NULL)
        c->init_func (rv);
      c = c->parent_class;
    }
  while (c != NULL);
  return rv;
}

DSK_INLINE_FUNCS void       dsk_object_unref (void *object)
{
  DskObject *o = object;
  if (o)
    {
      dsk_assert (o->ref_count > 0);
      dsk_assert (o->object_class->magic == DSK_OBJECT_CLASS_MAGIC);
      if (--(o->ref_count) == 0)
        {
          for (c = o->object_class; c; c = c->parent_class)
            if (c->finalize != NULL)
              c->finalize (o);
          dsk_free (o);
        }
    }
}

DSK_INLINE_FUNCS void      *dsk_object_ref   (void *object)
{
  DskObject *o = object;
  if (o)
    {
      dsk_assert (o->ref_count > 0);
      dsk_assert (o->object_class->magic == DSK_OBJECT_CLASS_MAGIC);
      o->ref_count += 1;
    }
  return o;
}
#endif

extern DskObjectClass dsk_object_class;
