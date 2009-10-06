#include <string.h>
#include "../dang.h"
#include "../dang-builtin-functions.h"
#include "../config.h"


typedef struct _GrepRunData GrepRunData;
struct _GrepRunData
{
  unsigned remaining;
  uint8_t *in_set;
  uint8_t *in_set_at;
  uint8_t in_set_mask;
  char *input_data_ptr;
  unsigned result_count;
};

static void
destruct__grep_run_data (DangValueType *type,
                           void *data)
{
  GrepRunData *rd = data;
  DANG_UNUSED (type);
  dang_free (rd->in_set);
}

DANG_C_FUNC_DECLARE (do_grep)
{
  DangValueType *element_type = func_data;
  unsigned size = element_type->sizeof_instance;
  GrepRunData *rd = state_data;
  DangVector *vector = * (DangVector **) args[0];
  DangFunction *f = * (DangFunction **) args[1];
  uint8_t sub_rv;
  if (f == NULL)
    {
      dang_set_error (error, "null-pointer exception");
      return DANG_C_FUNCTION_ERROR;
    }
  if (vector == NULL)
    {
      * (DangVector **) rv_out = NULL;
      return DANG_C_FUNCTION_SUCCESS;
    }
  if (rd->in_set == NULL)
    {
      /* first time */
      unsigned bit_size = vector->len / 8 + 1;
      rd->in_set = dang_malloc0 (bit_size);
      rd->in_set_at = rd->in_set;
      rd->in_set_mask = 1;
      rd->input_data_ptr = vector->data;
      rd->remaining = vector->len;
    }
  else
    {
      /* copy old value in */
      dang_c_function_end_subcall (thread, f, NULL, &sub_rv);
      if (sub_rv)
        {
          *rd->in_set_at |= rd->in_set_mask;
          rd->result_count++;
        }
      rd->in_set_mask <<= 1;
      if (rd->in_set_mask == 0)
        {
          rd->in_set_mask = 1;
          rd->in_set_at++;
        }
      rd->input_data_ptr += size;
      rd->remaining--;
    }

  if (rd->remaining > 0)
    {
      /* setup input args */
      return dang_c_function_begin_subcall (thread, f, (void **) &rd->input_data_ptr);
    }


  /* finish */
  DangVector *rv;
  uint8_t *out;
  const uint8_t *in;
  const uint8_t *in_bits;
  uint8_t in_mask;

  rv = * (DangVector **) rv_out = dang_new (DangVector, 1);
  rv->len = rd->result_count;
  rv->data = dang_malloc (rd->result_count * size);
  out = rv->data;
  in = vector->data;
  in_bits = rd->in_set;
  in_mask = 1;
  if (element_type->init_assign)
    {
      unsigned remaining = vector->len;
      while (remaining--)
        {
          if (*in_bits & in_mask)
            {
              element_type->init_assign (element_type, out, in);
              out += size;
            }
          in += size;
          in_mask <<= 1;
          if (in_mask == 0)
            {
              in_mask = 1;
              in_bits++;
            }
        }
    }
  else
    {
      unsigned remaining = vector->len;
      while (remaining--)
        {
          if (*in_bits & in_mask)
            {
              memcpy (out, in, size);
              out += size;
            }
          in += size;
          in_mask <<= 1;
          if (in_mask == 0)
            {
              in_mask = 1;
              in_bits++;
            }
        }
    }
  dang_free (rd->in_set);
  rd->in_set = NULL;
  return DANG_C_FUNCTION_SUCCESS;
}

DangFunction *dang_builtin_function_grep        (DangValueType *tensor_type)
{
  DangFunctionParam fparams[2], arg_fparam;
  DangSignature *arg_sig;
  DangSignature *sig;
  DangFunction *rv;
  DangValueType *element_type = ((DangValueTypeTensor *) tensor_type)->element_type;
  static DangValueType *grep_state_type = NULL;
  dang_assert (((DangValueTypeTensor*) tensor_type)->rank == 1);
  dang_assert (dang_value_type_is_tensor (tensor_type));

  /* Construct the overall signature of this flavor of map. */
  arg_fparam.dir = DANG_FUNCTION_PARAM_IN;
  arg_fparam.type = element_type;
  arg_fparam.name = NULL;
  arg_sig = dang_signature_new (dang_value_type_boolean (), 1, &arg_fparam);
  fparams[0].dir = DANG_FUNCTION_PARAM_IN;
  fparams[0].name = NULL;
  fparams[0].type = tensor_type;
  fparams[1].dir = DANG_FUNCTION_PARAM_IN;
  fparams[1].name = NULL;
  fparams[1].type = dang_value_type_function (arg_sig);
  dang_signature_unref (arg_sig);

  if (grep_state_type == NULL)
    {
      grep_state_type = dang_new0 (DangValueType, 1);
      grep_state_type->destruct = destruct__grep_run_data;
      grep_state_type->sizeof_instance = sizeof (GrepRunData);
      grep_state_type->alignof_instance = DANG_ALIGNOF_POINTER;
      grep_state_type->full_name = "internal-grep-state";
    }

  sig = dang_signature_new (tensor_type, 2, fparams);
  rv = dang_function_new_c (sig, grep_state_type, do_grep, element_type, NULL);
  dang_signature_unref (sig);
  return rv;
}
