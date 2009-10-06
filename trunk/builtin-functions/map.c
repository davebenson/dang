#include <string.h>
#include "../dang.h"
#include "../dang-builtin-functions.h"
#include "../config.h"

#define MAX_MAP_ARGS            32

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
  unsigned remaining;
  DangValueType *output_type;           /* element type */
  unsigned output_size;
  DangTensor *constructing;
  char *start_output_data_ptr;
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
destruct__tensor_run_data (DangValueType *type,
                           void *data)
{
  TensorMapRunData *rd = data;
  DANG_UNUSED (type);
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
run_data_advance        (TensorMapData *md,
                         TensorMapRunData *rd)
{
  unsigned i;
  unsigned *sizes = TMRD_GET_SIZES (rd, md);
  for (i = 0; i < md->n_inputs; i++)
    rd->input_data_ptrs[i] += sizes[i];
  rd->output_data_ptr += rd->output_size;
  rd->remaining--;
}


DANG_C_FUNC_DECLARE (do_tensor_map)
{
  TensorMapData *md = func_data;
  TensorMapRunData *rd = state_data;
  unsigned *sizes = TMRD_GET_SIZES (rd, md);
  DangFunction *f = * (DangFunction **) args[md->n_inputs];
  if (f == NULL)
    {
      dang_set_error (error, "null-pointer exception");
      return DANG_C_FUNCTION_ERROR;
    }
  if (rd->constructing == NULL)
    {
      /* first time */
      unsigned n_inputs = md->n_inputs;
      unsigned total_size = 1;
      unsigned i, d;
      DangValueType *elt_type = md->output_type;
      DangTensor **inputs = dang_newa (DangTensor *, n_inputs);
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
                  return DANG_C_FUNCTION_ERROR;
                }
            }
        }
      for (i = 0; i < n_inputs; i++)
        {
          rd->input_data_ptrs[i] = inputs[i]->data;
          sizes[i] = md->input_types[i]->sizeof_instance;
        }

      rd->output_data_ptr = dang_malloc (total_size * elt_type->sizeof_instance);
      rd->start_output_data_ptr = rd->output_data_ptr;
      rd->output_size = elt_type->sizeof_instance;
      rd->remaining = total_size;
    }
  else
    {
      /* copy old value in */
      dang_c_function_end_subcall (thread, f, NULL, rd->output_data_ptr);
      run_data_advance (md, rd);
    }

  if (rd->remaining > 0)
    {
      /* setup input args */
      return dang_c_function_begin_subcall (thread, f, (void **) rd->input_data_ptrs);
    }

  * (DangTensor **) rv_out = rd->constructing;
  rd->constructing = NULL;
  return DANG_C_FUNCTION_SUCCESS;
}

DangFunction *dang_builtin_function_map_tensors (unsigned n_tensor_args,
					         DangValueType **tensor_types,
                                                 DangValueType *output_tensor_type)
{
  /* Construct the tensor_map_data */
  TensorMapData *tensor_map_data;
  unsigned i;
  unsigned rank;
  DangFunctionParam *fparams;
  static DangValueType *map_state_types[MAX_MAP_ARGS + 1];
  DangSignature *sig;
  DangFunction *rv;
  if (n_tensor_args > MAX_MAP_ARGS)
    {
      dang_die ("two many arguments to 'map' (FIXME)");
    }

  tensor_map_data = dang_new (TensorMapData, 1);
  tensor_map_data->n_inputs = n_tensor_args;
  tensor_map_data->input_types = dang_new (DangValueType *, tensor_map_data->n_inputs);
  for (i = 0; i < tensor_map_data->n_inputs; i++)
    {
      DangValueTypeTensor *ttype = (DangValueTypeTensor*)tensor_types[i];
      tensor_map_data->input_types[i] = ttype->element_type;
      if (i == 0)
        rank = ttype->rank;
      else
        dang_assert (ttype->rank == rank);
    }
  {
    DangValueTypeTensor *ttype = (DangValueTypeTensor*)output_tensor_type;
    tensor_map_data->output_type = ttype->element_type;
    dang_assert (ttype->rank == rank);
  }
  tensor_map_data->rank = rank;

  /* Construct the overall signature of this flavor of map. */
  for (i = 0; i < n_tensor_args; i++)
    {
      fparams[i].dir = DANG_FUNCTION_PARAM_IN;
      fparams[i].type = tensor_types[i];
      fparams[i].name = NULL;
    }

  if (map_state_types[n_tensor_args] == NULL)
    {
      DangValueType *type = dang_new0 (DangValueType, 1);
      type->destruct = destruct__tensor_run_data;
      type->sizeof_instance = sizeof (TensorMapRunData)
                            + sizeof (void *) * (n_tensor_args - 1)
                            + sizeof (unsigned) * n_tensor_args;
      type->alignof_instance = DANG_ALIGNOF_POINTER;
      type->sizeof_instance = DANG_ALIGN (type->sizeof_instance, DANG_ALIGNOF_POINTER);
      type->full_name = "internal-map-state";
      map_state_types[n_tensor_args] = type;
    }


  sig = dang_signature_new (output_tensor_type, n_tensor_args, fparams);
  rv = dang_function_new_c (sig,
                            map_state_types[n_tensor_args],
                            do_tensor_map,
                            tensor_map_data,
                            free_tensor_map_data);
  dang_signature_unref (sig);
  return rv;
}
