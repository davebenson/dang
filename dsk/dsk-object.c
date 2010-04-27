#include "dsk-common.h"
#include "dsk-object.h"

#define ASSERT_OBJECT_CLASS_MAGIC(class) \
  dsk_assert ((class)->object_class_magic == DSK_OBJECT_CLASS_MAGIC)

static void
dsk_object_init (DskObject *object)
{
  ASSERT_OBJECT_CLASS_MAGIC (object->object_class);
}

static void
dsk_object_finalize (DskObject *object)
{
  ASSERT_OBJECT_CLASS_MAGIC (object->object_class);
  dsk_assert (object->ref_count == 0);
}

DskObjectClass dsk_object_class =
DSK_OBJECT_CLASS_DEFINE(DskObject, NULL, dsk_object_init, dsk_object_finalize);

dsk_boolean
dsk_object_is_a (void *object,
                 void *isa_class)
{
  DskObject *o = object;
  DskObjectClass *c;
  DskObjectClass *ic = isa_class;
  if (o == NULL)
    return DSK_FALSE;
  ASSERT_OBJECT_CLASS_MAGIC (ic);
  /* Deliberately ignore ref_count, since we want to be able to cast during
     finalization, i guess */
  c = o->object_class;
  ASSERT_OBJECT_CLASS_MAGIC (c);
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
   || o->object_class->object_class_magic != DSK_OBJECT_CLASS_MAGIC)
    return "invalid-object";
  return o->object_class->name;
}

static const char *
dsk_object_class_get_name (DskObjectClass *class)
{
  if (class->object_class_magic == DSK_OBJECT_CLASS_MAGIC)
    return class->name;
  else
    return "*invalid-object-class*";
}

void *
dsk_object_cast (void       *object,
                 void       *isa_class,
                 const char *filename,
                 unsigned    line)
{
  if (!dsk_object_is_a (object, isa_class))
    {
      dsk_warning ("attempt to cast object %p (type %s) to %s invalid (%s:%u)",
                   object, dsk_object_get_class_name (object),
                   dsk_object_class_get_name (isa_class),
                   filename, line);
    }
  return object;
}

void *
dsk_object_cast_get_class (void       *object,
                           void       *isa_class,
                           const char *filename,
                           unsigned    line)
{
  if (!dsk_object_is_a (object, isa_class))
    {
      dsk_warning ("attempt to get-class for object %p (type %s) to %s invalid (%s:%u)",
                   object, dsk_object_get_class_name (object),
                   dsk_object_class_get_name (isa_class),
                   filename, line);
    }
  return ((DskObject*)object)->object_class;
}

void
dsk_object_handle_last_unref (DskObject *o)
{
  DskObjectClass *c = o->object_class;
  ASSERT_OBJECT_CLASS_MAGIC (c);
  do
    {
      if (c->finalize != NULL)
        c->finalize (o);
      c = c->parent_class;
    }
  while (c != NULL);
  dsk_free (o);
}


void *
dsk_object_ref_f (void *object)
{
  return dsk_object_ref (object);
}

void
dsk_object_unref_f (void *object)
{
  dsk_object_unref (object);
}

void
dsk_object_weak_ref (DskObject *object,
                     DskDestroyNotify destroy,
                     void            *destroy_data)
{
  DskObjectFinalizeHandler *f = dsk_malloc (sizeof (DskObjectFinalizeHandler));
  f->destroy = destroy;
  f->destroy_data = destroy_data;
  f->next = object->finalizer_list;
  object->finalizer_list = f;
}

void
dsk_object_weak_unref(DskObject      *object,
                      DskDestroyNotify destroy,
                      void            *destroy_data)
{
  DskObjectFinalizeHandler **pf = &object->finalizer_list;
  while (*pf)
    {
      if ((*pf)->destroy == destroy && (*pf)->destroy_data == destroy_data)
        {
          DskObjectFinalizeHandler *die = *pf;
          *pf = die->next;
          dsk_free (die);
          return;
        }
      pf = &((*pf)->next);
    }
  dsk_return_if_reached ("no matching finalizer");
}
