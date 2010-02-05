
struct _DskObjectClass
{
  const char *name;
  DskObjectClass *parent_class;
  size_t sizeof_class;
  size_t sizeof_instance;
  unsigned object_class_magic;
  void (*finalize) (DskObject *object);
};
#define DSK_OBJECT_CLASS_MAGIC          0x159daf3f
#define DSK_OBJECT_CLASS_DEFINE(name, parent_class, finalize_func) \
       { #name, (DskObjectClass *) parent_class, \
         sizeof (name ## Class), sizeof (name), \
         DSK_OBJECT_CLASS_MAGIC, finalize_func }


struct _DskObject
{
  DskObjectClass *object_class;
  unsigned ref_count;
};

DSK_INLINE_FUNCS void       dsk_object_unref (void *object);
DSK_INLINE_FUNCS DskObject *dsk_object_ref   (void *object);
                 dsk_boolean dsk_object_is_a (void *object,
                                              void *isa_class);
                 void       *dsk_object_cast (void *object,
                                              void *isa_class,
                                              const char *filename,
                                              unsigned line);
#ifdef DSK_DISABLE_CAST_CHECKS
#define DSK_OBJECT_CAST(type, object, isa_class) ((type*)(object))
#else
#define DSK_OBJECT_CAST(type, object, isa_class) \
  ((type*)dsk_object_cast(object, isa_class, __FILE__, __LINE__))
#endif

#if DSK_CAN_INLINE || DSK_IMPLEMENT_INLINES

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
