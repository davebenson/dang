#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "dsk-common.h"
void dsk_error(const char *format, ...)
{
  va_list args;
  fprintf (stderr, "ERROR: ");
  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
  fprintf (stderr, "\n");
  abort ();
}
void dsk_warning(const char *format, ...)
{
  va_list args;
  fprintf (stderr, "WARNING: ");
  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
  fprintf (stderr, "\n");
}

void *dsk_malloc (size_t size)
{
  void *rv;
  if (size == 0)
    return NULL;
  rv = malloc (size);
  if (rv == NULL)
    dsk_error ("out-of-memory allocating %u bytes", (unsigned) size);
  return rv;
}
void *dsk_malloc0 (size_t size)
{
  void *rv;
  if (size == 0)
    return NULL;
  rv = malloc (size);
  if (rv == NULL)
    dsk_error ("out-of-memory allocating %u bytes", (unsigned) size);
  memset (rv, 0, size);
  return rv;
}
void  dsk_free (void *ptr)
{
  if (ptr)
    free (ptr);
}
void *dsk_realloc (void *ptr, size_t size)
{
  if (ptr == NULL)
    return dsk_malloc (size);
  else if (size == 0)
    {
      dsk_free (ptr);
      return NULL;
    }
  else
    {
      void *rv = realloc (ptr, size);
      if (rv == NULL)
        dsk_error ("out-of-memory re-allocating %u bytes", (unsigned) size);
      return rv;
    }
}

char *dsk_strdup (const char *str)
{
  if (str == NULL)
    return NULL;
  else
    {
      unsigned len = strlen (str);
      char *rv = dsk_malloc (len + 1);
      memcpy (rv, str, len + 1);
      return rv;
    }
}
void dsk_bzero_pointers (void *ptrs, unsigned n_ptrs)
{
  void **at = ptrs;
  while (n_ptrs--)
    *at++ = NULL;
}
