#define _GNU_SOURCE
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dang.h"

static void out_of_memory (void)
{
  fprintf (stderr, "error: out of memory!\n\n");
  abort ();
}

void *dang_malloc   (size_t size)
{
  void *rv;
  if (size == 0)
    return NULL;
  rv = malloc (size);
  if (DANG_UNLIKELY (rv == NULL))
    out_of_memory ();
  return rv;
}

void *dang_malloc0  (size_t size)
{
  return memset (dang_malloc (size), 0, size);
}

void  dang_free     (void *ptr)
{
  if (ptr)
    free (ptr);
}
void *dang_realloc  (void *ptr, size_t size)
{
  if (ptr == NULL)
    return dang_malloc (size);
  else if (size == 0)
    {
      dang_free (ptr);
      return NULL;
    }
  else if (DANG_UNLIKELY ((ptr=realloc (ptr, size)) == NULL))
    {
      out_of_memory ();
      return NULL;
    }
  else
    return ptr;
}

char *dang_strdup   (const char *str)
{
  if (str == NULL)
    return NULL;
  else
    {
      unsigned len = strlen (str) + 1;
      return memcpy (dang_malloc (len), str, len);
    }
}
char *dang_strndup   (const char *str, size_t len)
{
  char *rv = dang_malloc (len + 1);
  memcpy (rv, str, len);
  rv[len] = 0;
  return rv;
}
void *dang_memdup   (const void *data, size_t len)
{
  void *rv = dang_malloc (len);
  memcpy (rv, data, len);
  return rv;
}
uint32_t dang_str_hash (const char *str)
{
  unsigned rv = 5003;
  while (*str)
    {
      rv += (uint8_t) *str++;
      rv *= 33;
    }
  return rv;
}
char *dang_strdup_vprintf (const char *format,
                           va_list     args)
{
  char *rv = NULL;

  /* NOTE THAT THIS IS MEMORY ALLOCATION ENTRY POINT!!!! */
  vasprintf (&rv, format, args);

  if (rv == NULL)
    out_of_memory ();
  return rv;
}
char *dang_strdup_printf (const char *format,
                          ...)
{
  va_list args;
  char *rv;
  va_start (args, format);
  rv = dang_strdup_vprintf (format, args);
  va_end (args);
  return rv;
}
dang_boolean dang_util_is_zero (const void *mem,
                                unsigned    len)
{
  const char *a = mem;
  while (len-- != 0)
    if (*a++ != 0)
      return FALSE;
  return TRUE;
}
unsigned dang_util_uint_product (unsigned N, const unsigned *terms)
{
  if (N == 0)
    return 1;
  else
    {
      unsigned rv = terms[0], i;
      for (i = 1; i < N; i++)
        rv *= terms[i];
      return rv;
    }
}

void dang_fatal_user_error (const char *format,
                            ...)
{
  char buf[1024];
  va_list args;
  va_start (args, format);
  vsnprintf (buf, sizeof (buf), format, args);
  va_end (args);
  buf[sizeof(buf)-1] = 0;
  fprintf (stderr, "ERROR: %s\n\n", buf);
  exit (1);
}

void dang_die (const char *format,
               ...)
{
  char buf[1024];
  va_list args;
  va_start (args, format);
  vsnprintf (buf, sizeof (buf), format, args);
  va_end (args);
  buf[sizeof(buf)-1] = 0;
  fprintf (stderr, "ERROR: %s\n\n", buf);
  abort ();
}

void dang_warning (const char *format,
               ...)
{
  char buf[1024];
  va_list args;
  va_start (args, format);
  vsnprintf (buf, sizeof (buf), format, args);
  va_end (args);
  buf[sizeof(buf)-1] = 0;
  fprintf (stderr, "WARNING: %s\n", buf);
}
void dang_printerr (const char *format, ...)
{
  va_list args;
  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
}


char *dang_util_join_with_dot (unsigned  n_names,
                               char    **names)
{
  unsigned total = 0;
  char *rv;
  dang_assert (n_names > 0);
  unsigned i;
  char *at;
  for (i = 0; i < n_names; i++)
    total += strlen (names[i]) + 1;
  rv = dang_malloc (total);
  at = rv;
  for (i = 0; i + 1 < n_names; i++)
    {
      at = stpcpy (at, names[i]);
      *at++ = '.';
    }
  strcpy (at, names[i]);
  return rv;
}
char **dang_util_split_by_dot (const char *str,
                               unsigned   *n_names_out)
{
  const char *at;
  unsigned n_dots = 0, len = 0;
  char **rv;
  char *str_heap;
  for (at = str; *at; at++)
    {
      if (*at == '.')
        n_dots++;
      len++;
    }

  /* We will have (n_dots+1) strings at we will need len + 1 bytes of string heap */
  rv = dang_malloc (sizeof(char*) * (n_dots + 1) + len + 1);
  str_heap = (char*)(rv + n_dots + 1);
  rv[0] = str_heap;
  n_dots = 0;
  for (at = str; *at; at++)
    {
      if (*at == '.')
        {
          *str_heap++ = 0;
          rv[++n_dots] = str_heap;
        }
      else
        *str_heap++ = *at;
    }
  *str_heap = 0;

  *n_names_out = n_dots + 1;
  return rv;
}


char *dang_util_c_escape (unsigned len,
                          const void *data,
                          dang_boolean include_quotes)
{
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  unsigned i;
  if (include_quotes)
    dang_string_buffer_append (&buf, "\"");
  for (i = 0; i < len; i++)
    {
      uint8_t c = ((uint8_t *) data)[i];
      if (!isprint (c) || c <= 27 || c == '"' || c == '\\')
	{
	  switch (c)
	    {
	    case '\t':
	      dang_string_buffer_append (&buf, "\\t");
	      break;
	    case '\r':
	      dang_string_buffer_append (&buf, "\\r");
	      break;
	    case '\n':
	      dang_string_buffer_append (&buf, "\\n");
	      break;
	    case '\\':
	      dang_string_buffer_append (&buf, "\\\\");
	      break;
	    case '"':
	      dang_string_buffer_append (&buf, "\\\"");
	      break;
	    default:
              {
                /* if the next character is a digit,
                   we must use a 3-digit code.
                   at the end-of-string we use a 3-digit to be careful
                   so that two escaped strings can be concatenated. */
                if (i + 1 == len || isdigit (((uint8_t*)data)[1]))
                  dang_string_buffer_printf (&buf, "\\%03o", c);
                else
                  dang_string_buffer_printf (&buf, "\\%o", c);
                break;
              }
	    }
	}
      else
	{
	  dang_string_buffer_append_c (&buf, c);
	}
    }
  if (include_quotes)
    dang_string_buffer_append (&buf, "\"");
  return buf.str;
}

/* --- strings --- */
DangString *dang_string_new  (const char *str)
{
  DangString *rv;
  unsigned len = strlen (str);
  rv = dang_malloc (sizeof (DangString) + len + 1);
  rv->ref_count = 1;
  rv->len = len;
  rv->str = (char *)(rv+1);
  memcpy (rv->str, str, rv->len + 1);
  return rv;
}
DangString *dang_string_new_raw  (unsigned len)
{
  DangString *rv;
  rv = dang_malloc (sizeof (DangString) + len + 1);
  rv->ref_count = 1;
  rv->len = len;
  rv->str = (char *)(rv+1);
  rv->str[len] = 0;
  return rv;
}

DangString *dang_string_new_len  (const char *str,
                                  unsigned len)
{
  DangString *rv = dang_string_new_raw (len);
  memcpy (rv->str, str, rv->len);
  return rv;
}

/*DangString *dang_string_new_printf (const char *str, ...) DANG_GNUC_PRINTF(1,2);*/
void        dang_string_unref(DangString *str)
{
  //dang_warning ("dang_string_unref: %p:%u: %s", str,str->ref_count,str->str);
  if (--(str->ref_count) == 0)
    dang_free (str);
}
DangString *dang_string_ref  (DangString *str)
{
  //dang_warning ("dang_string_ref: %p:%u: %s", str,str->ref_count,str->str);
  ++(str->ref_count);
  return str;
}
/* for debugging, copy the string when debugging, ref-count otherwise */
DangString *dang_string_ref_copy  (DangString *str)
{
#if defined(DANG_DEBUG)
  str = dang_string_new (str->str);
  //dang_warning ("dang_string_ref_copy: %p:%u: %s", str,str->ref_count,str->str);
#else
  ++(str->ref_count);
#endif
  return str;
}
DangString *dang_strings_concat (unsigned N,
                                 DangString **strs)
{
  unsigned i, len = 0;
  DangString *rv;
  for (i = 0; i < N; i++)
    len += strs[i] ? strs[i]->len : 0;
  rv = dang_malloc (sizeof (DangString) + len + 1);
  rv->ref_count = 1;
  rv->len = len;
  rv->str = (char *)(rv+1);

  len = 0;
  for (i = 0; i < N; i++)
    {
      memcpy (rv->str + len, strs[i]->str, strs[i]->len);
      len += strs[i]->len;
    }
  rv->str[len] = 0;
  return rv;
}
DangString *dang_string_joinv (DangString *delim,
                               unsigned N,
                               DangString **strs)
{
  if (delim == NULL || delim->len == 0)
    return dang_strings_concat (N, strs);
  else if (N == 0)
    return dang_string_new ("");
  else if (N == 1)
    return strs[0] ? dang_string_ref_copy (strs[0]) : dang_string_new ("");
  else
    {
      unsigned len = strs[0] ? strs[0]->len : 0;
      unsigned dlen = delim->len;
      unsigned i;
      DangString *rv;
      char *at;
      for (i = 1; i < N; i++)
        len += (strs[i] ? strs[i]->len : 0) + dlen;
      rv = dang_malloc (sizeof (DangString) + len + 1);
      at = (char*)(rv + 1);
      if (strs[0])
        {
          memcpy (at, strs[0]->str, strs[0]->len);
          at += strs[0]->len;
        }
      for (i = 1; i < N; i++)
        {
          memcpy (at, delim->str, dlen);
          at += dlen;
          if (strs[i])
            {
              memcpy (at, strs[i]->str, strs[i]->len);
              at += strs[i]->len;
            }
        }
      *at = 0;
      rv->ref_count = 1;
      rv->len = len;
      rv->str = (char *)(rv+1);
      return rv;
    }
}

DangString *dang_string_peek_boolean (dang_boolean b)
{
  static DangString strs[2] = {
    { 1, 5, "false" },
    { 1, 4, "true" }
  };
  return &strs[b ? 1 : 0];
}

/* --- string-buffer --- */
void
dang_string_buffer_vprintf (DangStringBuffer *buffer,
                            const char *format,
                            va_list args)
{
  char buf[2048];
  vsnprintf (buf, sizeof (buf), format, args);
  buf[sizeof(buf) - 1] = 0;
  dang_string_buffer_append (buffer, buf);
}

void
dang_string_buffer_printf (DangStringBuffer *buffer,
                           const char *format,
                           ...)
{
  va_list args;
  va_start (args, format);
  dang_string_buffer_vprintf (buffer, format, args);
  va_end (args);
}

static void
ensure_space_for_append (DangStringBuffer *buffer,
                         unsigned          len)
{
  if (buffer->len + len + 1 > buffer->alloced)
    {
      unsigned needed = buffer->len + len + 1;
      unsigned new_alloced = buffer->alloced;
      if (new_alloced == 0)
        new_alloced = 16;
      else
        new_alloced = new_alloced * 2;
      while (new_alloced < needed)
        new_alloced += new_alloced;
      buffer->str = dang_realloc (buffer->str, new_alloced);
      buffer->alloced = new_alloced;
    }
}

void dang_string_buffer_append_len (DangStringBuffer *buffer,
                                    const char       *str,
                                    unsigned          len)
{
  ensure_space_for_append (buffer, len);
  memcpy (buffer->str + buffer->len, str, len);
  buffer->len += len;
  buffer->str[buffer->len] = 0;
}

void dang_string_buffer_append (DangStringBuffer *buffer,
                                const char       *str)
{
  dang_string_buffer_append_len (buffer, str, strlen (str));
}
void dang_string_buffer_append_c(DangStringBuffer *buffer,
                                char               c)
{
  dang_string_buffer_append_len (buffer, &c, 1);
}
void dang_string_buffer_append_repeated_char (DangStringBuffer *buffer,
                                    char c,
                                    unsigned          len)
{
  ensure_space_for_append (buffer, len);
  memset (buffer->str + buffer->len, c, len);
  buffer->len += len;
  buffer->str[buffer->len] = 0;
}

/* --- arrays --- */
void dang_util_array_init            (DangUtilArray   *array,
                                 size_t       elt_size)
{
  array->len = 0;
  array->data = NULL;
  array->alloced = 0;
  array->elt_size = elt_size;
}
void dang_util_array_append          (DangUtilArray   *array,
                                 unsigned     count,
                                 const void  *data)
{
  if (array->len + count > array->alloced)
    {
      unsigned needed = array->len + count;
      unsigned new_size = array->alloced ? array->alloced * 2 : 4;
      while (new_size < needed)
        new_size *= 2;
      array->data = dang_realloc (array->data, new_size * array->elt_size);
      array->alloced = new_size;
    }
  memcpy ((char*)array->data + array->elt_size * array->len,
          data, count * array->elt_size);
  array->len += count;
}

void dang_util_array_set_size        (DangUtilArray   *array,
                                 unsigned     size)
{
  if (size > array->alloced)
    {
      unsigned new_size = array->alloced ? array->alloced * 2 : 16;
      while (new_size < size)
        new_size *= 2;
      array->data = dang_realloc (array->data, new_size * array->elt_size);
      array->alloced = new_size;
    }
  array->len = size;
}

void dang_util_array_set_size0       (DangUtilArray   *array,
                                 unsigned     size)
{
  if (size > array->len)
    {
      unsigned old_len = array->len;
      dang_util_array_set_size (array, size);
      memset ((char*)array->data + old_len * array->elt_size, 0, (size - old_len) * array->elt_size);
    }
  else if (size < array->len)
    dang_util_array_set_size (array, size);
}


void dang_util_array_remove (DangUtilArray *array,
                        unsigned   start,
                        unsigned   count)
{
  dang_assert (start + count <= array->len);
  memmove ((char*)array->data + start * array->elt_size,
           (char*)array->data + (start+count) * array->elt_size,
           (array->len - start - count) * array->elt_size);
  array->len -= count;
}

void dang_util_array_insert          (DangUtilArray   *array,
                                 unsigned     n,
                                 const void  *data,
                                 unsigned     insert_pos)
{
  unsigned old_len = array->len;
  dang_util_array_set_size (array, array->len + n);
  memmove ((char*)array->data + (insert_pos + n) * array->elt_size,
           (char*)array->data + (insert_pos) * array->elt_size,
           (old_len - insert_pos) * array->elt_size);
  memcpy ((char*)array->data + (insert_pos) * array->elt_size,
          data,
          n * array->elt_size);
}

void dang_util_array_clear           (DangUtilArray   *array)
{
  dang_free (array->data);
}

/* --- errors --- */
DangError *dang_error_newv       (const char *format,
                                  va_list     args)
{
  char *msg;
  DangError *rv;
  msg = dang_strdup_vprintf (format, args);
  rv = dang_new0 (DangError, 1);
  rv->ref_count = 1;
  rv->message = msg;
  return rv;
}
DangError *dang_error_new        (const char *format,
                                  ...)
{
  DangError *rv;
  va_list args;
  va_start (args, format);
  rv = dang_error_newv (format, args);
  va_end (args);
  return rv;
}

void       dang_set_error        (DangError **error,
                                  const char *format,
                                  ...)
{
  va_list args;
  dang_assert (error);
  dang_assert (*error == NULL); /* to consider: instead join them */
  va_start (args, format);
  *error = dang_error_newv (format, args);
  va_end (args);
}

void       dang_error_add_prefix (DangError  *error,
                                  const char *format,
                                  ...)
{
  char *msg, *msg2;
  va_list args;
  va_start (args, format);
  msg = dang_strdup_vprintf (format, args);
  va_end (args);
  msg2 = dang_strdup_printf ("%s: %s", msg, error->message);
  dang_free (msg);
  dang_free (error->message);
  error->message = msg2;
}

void       dang_error_add_suffix (DangError  *error,
                                  const char *format,
                                  ...)
{
  char *msg, *msg2;
  va_list args;
  va_start (args, format);
  msg = dang_strdup_vprintf (format, args);
  va_end (args);
  msg2 = dang_strdup_printf ("%s: %s", error->message, msg);
  dang_free (msg);
  dang_free (error->message);
  error->message = msg2;
}

void
dang_error_add_pos_suffix (DangError *error,
                           DangCodePosition *cp)
{
  dang_error_add_suffix (error, " ("DANG_CP_FORMAT")", DANG_CP_ARGS (*cp));
}

DangError *dang_error_ref        (DangError  *error)
{
  ++(error->ref_count);
  return error;
}

void       dang_error_unref      (DangError  *error)
{
  if (--(error->ref_count) == 0)
    {
      dang_free (error->message);
      dang_free (error->backtrace);
      if (error->filename != NULL)
        dang_string_unref (error->filename);
      dang_free (error);
    }
}


/* --- dang-string-table --- */

void *dang_string_table_lookup (void *top_node,
                                const char *name);
void  dang_string_table_insert (void **top_node,
                                void  *tree_node,
                                void **conflict_out);
