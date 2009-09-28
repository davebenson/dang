#include <string.h>
#include "dang.h"

static DANG_SIMPLE_C_FUNC_DECLARE(do_n_chars)
{
  DangString *s = *(DangString**)args[0];
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  * (uint32_t*) rv_out = s ? dang_utf8_count_unichars (s->len, s->str) : 0;
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE(do_n_bytes)
{
  DangString *s = *(DangString**)args[0];
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  * (uint32_t*) rv_out = s ? s->len : 0;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_string_split)
{
  DangVector *rv;
  DangString *delim = *(DangString**)args[0];
  DangString *to_split = *(DangString**)args[1];
  const char *dstr = delim ? delim->str : "";
  const char *sstr = to_split ? to_split->str : "";
  DangString *tmp;
  DangArray strings = DANG_ARRAY_STATIC_INIT(DangString*);
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  if (dstr[0] == 0)
    {
      /* split into an array of characters (not bytes) */
      unsigned dlen = strlen (dstr);
      const char *n = *sstr ? dang_utf8_next_char (sstr) : NULL;
      if (n == NULL)
        {
        }
      else
        {
          tmp = dang_string_new_len (sstr, n-sstr);
          dang_array_append (&strings, 1, &tmp);
          sstr = n + dlen;
          for (;;)
            {
              n = *sstr ? dang_utf8_next_char (sstr) : NULL;
              if (n == NULL)
                {
                  break;
                }
              else
                {
                  tmp = dang_string_new_len (sstr, n-sstr);
                  dang_array_append (&strings, 1, &tmp);
                  sstr = n + dlen;
                }
            }
        }
    }
  else
    {
      /* split "as usual" */
      unsigned dlen = strlen (dstr);
      const char *n = strstr (sstr, dstr);
      if (n == NULL)
        {
          tmp = to_split ? dang_string_ref_copy (to_split) : NULL;
          dang_array_append (&strings, 1, &tmp);
        }
      else
        {
          tmp = dang_string_new_len (sstr, n-sstr);
          dang_array_append (&strings, 1, &tmp);
          sstr = n + dlen;
          for (;;)
            {
              n = strstr (sstr, dstr);
              if (n == NULL)
                {
                  tmp = dang_string_new (sstr);
                  dang_array_append (&strings, 1, &tmp);
                  break;
                }
              else
                {
                  tmp = dang_string_new_len (sstr, n-sstr);
                  dang_array_append (&strings, 1, &tmp);
                  sstr = n + dlen;
                }
            }
        }

    }
  /* construct vector */
  rv = dang_new (DangVector, 1);
  rv->len = strings.len;
  rv->data = strings.data;
  rv->ref_count = 1;
  *(DangVector **) rv_out = rv;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_string_join)
{
  DangString *delim = * (DangString **) args[0];
  DangVector *b = *(DangVector**)args[1];
  DangString **strs = b->data;
  DANG_UNUSED (error);
  DANG_UNUSED (func_data);
  if (b == NULL)
    b = (DangVector *) dang_tensor_empty ();
  *(DangString**)rv_out = dang_string_joinv (delim, b->len, b->data);
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE(do_string_concat)
{
  DangVector *a = *(DangVector**) args[0];
  DANG_UNUSED (error);
  DANG_UNUSED (func_data);
  if (a == NULL)
    a = (DangVector *) dang_tensor_empty ();
  *(DangString**)rv_out = dang_strings_concat (a->len, a->data);
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE(do_cast_to_char_array)
{
  DangString *in = *(DangString**)args[0];
  DangVector *out = rv_out;
  unsigned c;
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  if (in == NULL)
    c = 0;
  else
    c = dang_utf8_count_unichars (in->len, in->str);
  if (c == 0)
    {
      *(DangVector **) rv_out = NULL;
      return TRUE;
    }
  out = dang_new (DangVector, 1);
  out->ref_count = 1;
  out->len = c;
  out->data = dang_new (dang_unichar, c);
  *(DangVector**)rv_out = out;
  dang_utf8_string_to_unichars (in->len, in->str, out->data);
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_cast_to_byte_array)
{
  DangString *in = *(DangString**)args[0];
  DangVector *out;
  unsigned c = in ? in->len : 0;
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  if (c == 0)
    {
      * (DangVector **) rv_out = NULL;
      return TRUE;
    }
  out = dang_new (DangVector, 1);
  out->ref_count = 1;
  out->len = c;
  out->data = dang_memdup (in->str, in->len);
  *(DangVector**) rv_out = out;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_cast_to_string)
{
  DangVector *in = *(DangVector **) args[0];
  unsigned utf8_len;
  DangString *out;
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  if (in == NULL)
    in = (DangVector *) dang_tensor_empty ();
  utf8_len = dang_unichar_array_get_utf8_len (in->len, in->data);
  out = dang_string_new_raw (utf8_len);
  dang_unichar_array_to_utf8 (in->len, in->data, out->str);
  * (DangString**) rv_out = out;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_cast_to_string_from_bytes)
{
  DangVector *in = *(DangVector**) (args[0]);
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  if (in == NULL)
    in = (DangVector *) dang_tensor_empty ();
  if (!dang_utf8_validate_str (in->len, in->data, error))
    return FALSE;
  * (DangString**) rv_out = dang_string_new_len (in->data, in->len);
  return TRUE;
}

void
_dang_string_init (DangNamespace *def)
{
  dang_namespace_add_simple_c_from_params
        (def, "n_chars", do_n_chars,
         dang_value_type_uint32 (),
         1,
         DANG_FUNCTION_PARAM_IN, "str", dang_value_type_string ());
  dang_namespace_add_simple_c_from_params
        (def, "n_bytes", do_n_bytes,
         dang_value_type_uint32 (),
         1,
         DANG_FUNCTION_PARAM_IN, "str", dang_value_type_string ());
  dang_namespace_add_simple_c_from_params
        (def, "split", do_string_split,
         dang_value_type_vector (dang_value_type_string ()),
         2,
         DANG_FUNCTION_PARAM_IN, "delim", dang_value_type_string (),
         DANG_FUNCTION_PARAM_IN, "to_split", dang_value_type_string ());
  dang_namespace_add_simple_c_from_params
        (def, "join", do_string_join,
         dang_value_type_string (),
         2,
         DANG_FUNCTION_PARAM_IN, "delim", dang_value_type_string (),
         DANG_FUNCTION_PARAM_IN, "strs",
              dang_value_type_vector (dang_value_type_string ()));
  dang_namespace_add_simple_c_from_params
        (def, "concat", do_string_concat,
         dang_value_type_string (),
         1,
         DANG_FUNCTION_PARAM_IN, "strs",
              dang_value_type_vector (dang_value_type_string ()));
  dang_namespace_add_simple_c_from_params
        (def, "operator_cast__tensor_1__char", do_cast_to_char_array,
         dang_value_type_vector (dang_value_type_char ()),
         1,
         DANG_FUNCTION_PARAM_IN, "str", dang_value_type_string ());
  dang_namespace_add_simple_c_from_params
        (def, "operator_cast__tensor_1__uint8", do_cast_to_byte_array,
         dang_value_type_vector (dang_value_type_uint8 ()),
         1,
         DANG_FUNCTION_PARAM_IN, "str", dang_value_type_string ());
  dang_namespace_add_simple_c_from_params
        (def, "operator_cast__string", do_cast_to_string,
         dang_value_type_string (),
         1,
         DANG_FUNCTION_PARAM_IN, "str",
              dang_value_type_vector (dang_value_type_char ()));

  /* validates utf8 */
  dang_namespace_add_simple_c_from_params
        (def, "operator_cast__string", do_cast_to_string_from_bytes,
         dang_value_type_string (),
         1,
         DANG_FUNCTION_PARAM_IN, "str",
              dang_value_type_vector (dang_value_type_uint8 ()));

}
