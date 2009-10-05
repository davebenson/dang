#include <string.h>
#include "../dang.h"
#include "../dang-builtin-functions.h"

typedef struct _TensorMapData TensorMapData;
struct _TensorMapData
{
  unsigned n_inputs;
  DangValueType **input_types;          /* element types */
  DangValueType *output_type;           /* element type */
  unsigned rank;
};

typedef struct _TensorMapRunData TensorMapRunData;
struct _TensorMapRunData
{
  DangThread *yielded;
  unsigned remaining;
  DangValueType *output_type;           /* element type */
  unsigned output_size;
  DangTensor *constructing;
  char *output_data_ptr;
  char *input_data_ptrs[1];
  /* sizes follow */
};
#define TMRD_GET_SIZES(rd, md) \
  (unsigned*)(rd->input_data_ptrs + md->n_inputs)

static void
free_tensor_map_data (void *data)
{
  TensorMapData *md = data;
  dang_free (md->input_types);
  dang_free (md);
}

static void
destruct__tensor_run_data (void *data)
{
  TensorMapRunData *rd = data;
  if (rd->yielded)
    {
      dang_thread_cancel (rd->yielded);
      rd->yielded = NULL;
    }
  if (rd->constructing)
    {
      DangValueType *elt_type = rd->output_type;
      if (elt_type->destruct)
        {
          unsigned n_built = (rd->output_data_ptr - (char*)rd->constructing->data) / rd->output_size;
          dang_value_bulk_destruct (elt_type, rd->constructing->data, n_built);
        }
      dang_free (rd->constructing->data);
      dang_free (rd->constructing);
      rd->constructing = NULL;
    }
}

static inline void
run_data_advance (TensorMapData *md,
                  TensorMapRunData *rd)
{
  unsigned i;
  unsigned *sizes = TMRD_GET_SIZES (rd, md);
  for (i = 0; i < md->n_inputs; i++)
    rd->input_data_ptrs[i] += sizes[i];
  rd->output_data_ptr += rd->output_size;
  rd->remaining--;
}

static inline void
copy_output_data (TensorMapRunData *rd)
{
  dang_assert (rd->yielded->status == DANG_THREAD_STATUS_DONE);
  memcpy (rd->output_data_ptr, rd->yielded->rv_frame + 1, rd->output_size);
  memset (rd->yielded->rv_frame + 1, 0, rd->output_size);
}

DANG_C_FUNC_DECLARE (do_tensor_map)
{
  TensorMapData *md = func_data;
  TensorMapRunData *rd = state_data;
  unsigned *sizes = TMRD_GET_SIZES (rd, md);

  if (rd->constructing == NULL)
    {
      /* first time */
      unsigned n_inputs = md->n_inputs;
      DangFunction *func = *(DangFunction**)(args[n_inputs]);
      unsigned total_size = 1;
      unsigned i, d;
      DangValueType *elt_type = md->output_type;
      DangTensor **inputs = dang_newa (DangTensor *, n_inputs);
      if (func == NULL)
        {
          dang_set_error (error, "null-pointer exception");
          return FALSE;
        }
      for (i = 0; i < n_inputs; i++)
        {
          inputs[i] = *(DangTensor**)(args[i]);
          if (inputs[i] == NULL)
            inputs[i] = dang_tensor_empty ();
        }
      for (d = 0; d < md->rank; d++)
        {
          unsigned dim = inputs[0]->sizes[d];
          total_size *= dim;
          for (i = 1; i < n_inputs; i++)
            {
              unsigned this_dim = inputs[i]->sizes[d];
              if (dim != this_dim)
                {
                  dang_set_error (error, "dimension #%u differ between params #1 and #%u to tensor.map", dim+1, i+1);
                  return FALSE;
                }
            }
        }
      for (i = 0; i < n_inputs; i++)
        {
          rd->input_data_ptrs[i] = inputs[i]->data;
          sizes[i] = md->input_types[i]->sizeof_instance;
        }

      rd->output_data_ptr = dang_malloc (total_size * elt_type->sizeof_instance);
      rd->output_size = elt_type->sizeof_instance;
    }
  else
    {
      /* copy old value in */
      ...
    }

  while (rd->remaining > 0)
    {
      return dang_thread_c_start_call (running_thread, ...);
    }
  return DANG_C_FUNCTION_SUCCESS;
}

DangFunction *dang_builtin_function_map_tensors (unsigned rank,
                                                 unsigned n_tensor_args,
					         DangValueType **tensor_types,
                                                 DangValueType *output_tensor_type)
{
  /* Construct the tensor_map_data */
  TensorMapData *tensor_map_data;
  unsigned i;
  tensor_map_data = dang_new (TensorMapData, 1);
  tensor_map_data->n_inputs = n_tensors;
  tensor_map_data->input_types = dang_new (DangValueType *, tensor_map_data->n_inputs);
  for (i = 0; i < tensor_map_data->n_inputs; i++)
    tensor_map_data->input_types[i] = ((DangValueTypeTensor*)tensor_types[i])->element_type;
  tensor_map_data->output_type = ((DangValueTypeTensor*)output_tensor_type)->element_type;
  tensor_map_data->rank = rank;

  /* Construct the overall signature of this flavor of map. */
  fparams[query->n_elements - 1].dir = DANG_FUNCTION_PARAM_IN;
  fparams[query->n_elements - 1].name = NULL;
  fparams[query->n_elements - 1].type = dang_value_type_function (func_sig);
  sig = dang_signature_new (dang_value_type_tensor (func_sig->return_type, rank),
                            query->n_elements, fparams);
  rv = dang_function_new_c (sig, do_tensor_map,
                            tensor_map_data,
                            free_tensor_map_data);
  dang_signature_unref (sig);
  dang_signature_unref (func_sig);
}
