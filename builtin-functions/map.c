#include "../dang.h"
#include "../dang-builtin-functions.h"

typedef struct _TensorMapData TensorMapData;
struct _TensorMapData
{
  unsigned n_inputs;
  DangValueType **input_types;          /* element types */
  DangValueType *output_type;           /* element type */
  DangValueType *output_tensor_type;
  unsigned rank;
};
struct _TensorMapRunData
{
  DangThread *yielded;
  unsigned index;
  DangValueType *output_tensor_type;
  DangTensor *constructing;
  char *output_data_ptr;
  char *data_ptrs[1];
};

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
      dang_tensor_unref (rd->output_tensor_type, rd->constructing);
      rd->constructing = NULL;
    }
}
DANG_C_FUNC_DECLARE (do_tensor_map)
{
  TensorMapData *md = func_data;
  TensorMapRunData *rd = state_data;

  if (rd->constructing == NULL)
    {
      /* first time */
      unsigned n_inputs = md->n_inputs;
      char **at = dang_newa (char *, n_inputs);
      unsigned *elt_sizes = dang_newa (unsigned, n_inputs);
      DangFunction *func = *(DangFunction**)(args[n_inputs]);
      unsigned total_size = 1;
      unsigned i, j, d;
      DangTensor *rv;
      DangValueType *elt_type = md->output_type;
      char *rv_at;
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
          at[i] = inputs[i]->data;
          elt_sizes[i] = md->input_types[i]->sizeof_instance;
        }

      void *rv_data;
      if (elt_type->destruct)
        rv_data = dang_malloc0 (total_size * elt_type->sizeof_instance);
      else
        rv_data = dang_malloc (total_size * elt_type->sizeof_instance);
      rv_at = rv_data;

    }
  else
    {
      /* yielded:  handle error code, otherwise, load up variables. */
      ...
    }

  /* TODO: implement a way to recycle a thread
   *       to call the same function twice! */

  while (rd->remaining > 0)
    {
      --(rd->remaining);
      if (rd->thread == NULL)
        rd->thread = dang_thread_new (...);
      else
        dang_thread_reset (rd->thread, ...);
      for (;;)
        {
          dang_thread_run (rd->thread);
          switch (rd->thread->status)
            {
            case DANG_THREAD_STATUS_RUNNING:
              ...
              break;
            case DANG_THREAD_STATUS_YIELDED:
              ...
              break;
            case DANG_THREAD_STATUS_DONE:
              ...
              break;
            case DANG_THREAD_STATUS_THREW:
              ...
              break;
            case DANG_THREAD_STATUS_CANCELLED:
              ...
              break;
            }
        }
      for (j = 0; j < n_inputs; j++)
        rd->data_ptrs[j] += elt_sizes[j];
      rd->output_data_ptr += elt_type->sizeof_instance;
    }

  rv = dang_malloc (DANG_TENSOR_SIZEOF (md->rank));
  rv->ref_count = 1;
  for (d = 0; d < md->rank; d++)
    rv->sizes[d] = inputs[0]->sizes[d];
  rv->data = rv_data;
  *(DangTensor **)rv_out = rv;

  return TRUE;
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
  tensor_map_data->output_tensor_type = output_tensor_type;
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
