#include "dang.h"

static DangArray concat_functions = DANG_ARRAY_STATIC_INIT (DangFunction *);


static dang_boolean
do_concat (void      **args,
           void       *rv_out,
           void       *func_data,
           DangError **error)
{
  unsigned N = (size_t) func_data;
  DangString **strs = alloca (sizeof (DangString *) * N);
  unsigned i;
  for (i = 0; i < N; i++)
    if ((strs[i] = *(DangString**)(args[i])) == NULL)
      {
        dang_set_error (error, "null-pointer exception");
        return FALSE;
      }
  * (DangString**) rv_out = dang_strings_concat (N, strs);
  return TRUE;
}

DangFunction *
dang_function_concat_peek (unsigned N)
{
  DangFunction **prv;
  if (N >= concat_functions.len)
    dang_array_set_size0 (&concat_functions, N + 1);
  prv = ((DangFunction **) (concat_functions.data)) + N;
  if (*prv == NULL)
    {
      DangFunctionParam *params = dang_new (DangFunctionParam, N);
      unsigned i;
      DangSignature *sig;
      for (i = 0; i < N; i++)
        {
          params[i].dir = DANG_FUNCTION_PARAM_IN;
          params[i].type = dang_value_type_string ();
          params[i].name = NULL;
        }
      sig = dang_signature_new (dang_value_type_string (), N, params);
      *prv = dang_function_new_simple_c (sig, do_concat,
                                         (void*)N, NULL);
      dang_signature_unref (sig);
      dang_free (params);
    }
  return *prv;
}

void
_dang_function_concat_cleanup (void)
{
  unsigned i;
  DangFunction **functions = concat_functions.data;
  for (i = 0; i < concat_functions.len; i++)
    if (functions[i])
      dang_function_unref (functions[i]);
  dang_array_clear (&concat_functions);
  DANG_ARRAY_INIT (&concat_functions, DangFunction *);
}
