#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "dang.h"
#include "dang-file.h"

static DANG_SIMPLE_C_FUNC_DECLARE(file_open)
{
  DangFile *file = * (DangFile **) args[0];
  DangString *fname = * (DangString **) args[1];
  const char *flags = func_data;
  DANG_UNUSED (rv_out);
  if (fname == NULL || fname->len == 0)
    {
      dang_set_error (error, "empty filename (open flags=%s)", flags);
      return FALSE;
    }
  if (file->fp)
    fclose (file->fp);
  file->fp = fopen (fname->str, flags);
  if (file->fp == NULL)
    {
      dang_set_error (error, "error opening file %s (mode %s)",
                      fname->str, flags);
      return FALSE;
    }
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE(file_write)
{
  DangFile *file = * (DangFile **) args[0];
  DangString *str = * (DangString **) args[1];
  DANG_UNUSED (rv_out);
  DANG_UNUSED (func_data);
  if (str == NULL || str->len == 0)
    return TRUE;
  if (file->fp == NULL)
    {
      dang_set_error (error, "file not open");
      return FALSE;
    }
  if (fwrite (str->str, 1, str->len, file->fp) != str->len)
    {
      dang_set_error (error, "error write(): %s", strerror (errno));
      return FALSE;
    }
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE(file_writeln)
{
  DangFile *file = * (DangFile **) args[0];
  DangString *str = * (DangString **) args[1];
  DANG_UNUSED (rv_out);
  DANG_UNUSED (func_data);
  if (file->fp == NULL)
    {
      dang_set_error (error, "file not open");
      return FALSE;
    }
  if (str != NULL && str->len != 0
   && fwrite (str->str, 1, str->len, file->fp) != str->len)
    goto error;
  if (fputc ('\n', file->fp) == EOF)
    goto error;
  return TRUE;

error:
  dang_set_error (error, "error writeln(): %s", strerror (errno));
  return FALSE;
}


DANG_SIMPLE_C_FUNC_DECLARE(file_readln)
{
  DangFile *file = * (DangFile **) args[0];
  FILE *fp = file->fp;
  char buf[1025];
  DangStringBuffer str = DANG_STRING_BUFFER_INIT;
  DANG_UNUSED (func_data);
  if (fp == NULL)
    {
      dang_set_error (error, "file not open");
      return FALSE;
    }
  while (fgets(buf,sizeof(buf),fp))
    {
      dang_string_buffer_append (&str, buf);
      if (strchr (buf, '\n'))
        {
          str.len--;
          break;
        }
    }
  if (str.str == NULL)
    {
      if (ferror (fp))
        {
          dang_set_error (error, "error reading from file");
          return FALSE;
        }
      * (DangString **) rv_out = NULL;
      return TRUE;
    }
  * (DangString **) rv_out = dang_string_new_len (str.str, str.len);
  dang_free (str.str);
  return TRUE;
}

DANG_SIMPLE_C_FUNC_DECLARE(file_flush)
{
  DangFile *file = * (DangFile **) args[0];
  FILE *fp = file->fp;
  DANG_UNUSED (func_data);
  DANG_UNUSED (rv_out);
  if (fp == NULL)
    {
      dang_set_error (error, "file not open");
      return FALSE;
    }
  fflush (fp);
  return TRUE;
}

DANG_SIMPLE_C_FUNC_DECLARE(file_close)
{
  DangFile *file = * (DangFile **) args[0];
  DANG_UNUSED (func_data);
  DANG_UNUSED (rv_out);
  DANG_UNUSED (error);
  if (file->fp == NULL)
    return TRUE;
  fclose (file->fp);
  file->fp = NULL;
  return TRUE;
}

DANG_SIMPLE_C_FUNC_DECLARE(file_unlink)
{
  DangString *fname = * (DangString **) args[0];
  DANG_UNUSED (rv_out);
  DANG_UNUSED (func_data);
  if (fname == NULL || fname->len == 0)
    {
      dang_set_error (error, "unlink: empty filename");
      return FALSE;
    }
  if (unlink (fname->str) < 0)
    {
      dang_set_error (error, "unlink %s: %s", fname->str, strerror (errno));
      return FALSE;
    }
  return TRUE;
}

static struct {
  const char *ctor_name;
  const char *fopen_flags;
} constructor_info[3] = {
  { "read", "r" },
  { "write", "w" },
  { "append", "a" },
};

static void
add_method_simple_c (DangValueType *type,
                     const char    *method_name,
                     DangMethodFlags  flags,
                     DangSignature   *sig,
                     DangSimpleCFunc  f)
{
  DangError *error = NULL;
  DangFunction *func = dang_function_new_simple_c (sig, f, NULL, NULL);
  if (!dang_object_add_method (type, method_name, flags, func, &error))
    dang_die ("error adding method %s to %s", method_name, type->full_name);
  dang_function_unref (func);
}
void _dang_file_init (DangNamespace *ns)
{
  DangValueType *type;
  DangFunctionParam params[2];
  DangSignature *sig;
  DangFunction *func;
  unsigned i;
  type = dang_object_type_subclass (dang_value_type_object (), "File");
  if (!dang_namespace_add_type (ns, "File", type, NULL))
    assert(0);

  /* Allocate space for FILE* fp */
  if (!dang_object_add_member (type, "*fp*", 0,
                               dang_value_type_reserved_pointer (),
                               NULL, NULL)) assert(0);

  params[0].type = type;
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].name = "this";
  params[1].type = dang_value_type_string ();
  params[1].dir = DANG_FUNCTION_PARAM_IN;
  params[1].name = "filename";
  sig = dang_signature_new (NULL, 2, params);
  for (i = 0; i < DANG_N_ELEMENTS (constructor_info); i++)
    {
      func = dang_function_new_simple_c (sig, file_open, 
                                         (void*)constructor_info[i].fopen_flags, NULL);
      if (!dang_object_add_constructor (type, constructor_info[i].ctor_name,
                                        func, NULL))
        dang_die ("adding read constructing");
      dang_function_unref (func);
    }
  dang_signature_unref (sig);

  params[1].name = "str";

  sig = dang_signature_new (NULL, 2, params);
  add_method_simple_c (type, "write", DANG_METHOD_PUBLIC|DANG_METHOD_FINAL,
                       sig, file_write);
  add_method_simple_c (type, "writeln", DANG_METHOD_PUBLIC|DANG_METHOD_FINAL,
                       sig, file_writeln);
  dang_signature_unref (sig);

  sig = dang_signature_new (dang_value_type_string (), 1, params);
  add_method_simple_c (type, "readln", DANG_METHOD_PUBLIC|DANG_METHOD_FINAL,
                       sig, file_readln);
  dang_signature_unref (sig);

  sig = dang_signature_new (NULL, 1, params);
  add_method_simple_c (type, "close", DANG_METHOD_PUBLIC|DANG_METHOD_FINAL,
                       sig, file_close);
  add_method_simple_c (type, "flush", DANG_METHOD_PUBLIC|DANG_METHOD_FINAL,
                       sig, file_flush);
  dang_signature_unref (sig);

  params[0].type = dang_value_type_string ();
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].name = "filename";
  sig = dang_signature_new (NULL, 1, params);
  func = dang_function_new_simple_c (sig, file_unlink, NULL, NULL);
  dang_namespace_add_function (ns, "unlink", func, NULL);
  dang_function_unref (func);
  dang_signature_unref (sig);

}
