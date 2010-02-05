#include "dsk-object.h"

static void
dsk_object_finalize (DskObject *object)
{
  dsk_assert (object->object_class->magic == DSK_OBJECT_CLASS_MAGIC);
  dsk_assert (object->ref_count == 0);
  dsk_free (object);
}

DskObjectClass dsk_object_class = DSK_OBJECT_CLASS_DEFINE(DskObject, NULL, dsk_object_finalize);

dsk_boolean dsk_object_is_a (void *object, void *isa_class)
{
  DskObject *o = object;
  DskObjectClass *c;
  DskObjectClass *ic = isa_class;
  if (o == NULL)
    return DSK_FALSE;
  dsk_assert (ic->magic == DSK_OBJECT_CLASS_MAGIC);
  /* Deliberately ignore ref_count, since we want to be able to cast during
     finalization, i guess */
  c = o->object_class;
  dsk_assert (c->magic == DSK_OBJECT_CLASS_MAGIC);
  while (c != NULL)
    {
      if (c == ic)
        return DSK_TRUE;
      c = c->parent_class;
    }
  return DSK_FALSE;
}
static inline const char *
dsk_object_get_class_name (void *object)
{
  DskObject *o = object;
  if (o == NULL)
    return "*null*";
  if (o->object_class == NULL
   || o->object_class->magic != DSK_OBJECT_CLASS_MAGIC)
    return "invalid-object";
  return o->object_class->name;
}

DSK_INLINE_FUNCS void       *dsk_object_cast (void *object,
                                              void *isa_class,
                                              const char *filename,
                                              unsigned line)
{
  if (!dsk_object_is_a (object, isa_class))
    {
      dsk_warning ("attempt to cast object %p (type %s) to %s invalid (%s:%u)",
                   object, dsk_object_get_class_name (object),
                   dsk_object_class_get_name (isa_class),
                   filename, line);
    }
}
