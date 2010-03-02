#include <stdio.h>
#include "dsk-common.h"
#include "dsk-object.h"
#include "dsk-error.h"

static void dsk_error_finalize (DskError *error)
{
  dsk_free (error->message);
}

DskErrorClass dsk_error_class =
{
  DSK_OBJECT_CLASS_DEFINE(DskError, &dsk_object_class,
                          NULL,
                          dsk_error_finalize)
};


DskError *dsk_error_new        (const char *format,
                                ...)
{
  va_list args;
  DskError *rv;
  va_start (args, format);
  rv = dsk_error_new_valist (format, args);
  va_end (args);
  return rv;
}

DskError *dsk_error_new_valist (const char *format,
                                va_list     args)
{
  char buf[512];
  DskError *rv;
  vsnprintf (buf, sizeof (buf), format, args);
  buf[sizeof(buf) - 1] = 0;
  rv = dsk_object_new (&dsk_error_class);
  rv->message = dsk_strdup (buf);
  return rv;
}

DskError *dsk_error_new_literal(const char *message)
{
  DskError *rv = dsk_object_new (&dsk_error_class);
  rv->message = dsk_strdup (message);
  return rv;
}

void dsk_set_error (DskError **error,
                    const char *format,
                    ...)
{
  va_list args;
  if (error == NULL)
    return;

  dsk_assert (*error == NULL);

  va_start (args, format);
  *error = dsk_error_new_valist (format, args);
  va_end (args);
}

DskError *dsk_error_ref        (DskError   *error)
{
  dsk_assert (dsk_object_is_a (error, &dsk_error_class));
  return (DskError *) dsk_object_ref (error);
}

void      dsk_error_unref      (DskError   *error)
{
  dsk_assert (dsk_object_is_a (error, &dsk_error_class));
  dsk_object_unref (error);
}