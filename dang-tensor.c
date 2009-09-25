#include <string.h>
#include "dang.h"
#include "config.h"
#include "magic.h"
#include "gskrbtreemacros.h"

//$operator_index(a, I, J, ...)
//$map(TENSOR, BOUND_VAR, EXPR)

/* anything greater than this requires extra allocations */
#define MAX_STANDARD_RANK       16

static DangValueTypeTensor *tensor_type_tree;
#define GET_IS_RED(fi)  (fi)->is_red
#define SET_IS_RED(fi,v)  (fi)->is_red = v
#define COMPARE_TENSOR_TREE_NODES(a,b,rv) \
  if(a->rank < b->rank) rv = -1; \
  else if(a->rank > b->rank) rv = 1; \
  else if(a->element_type < b->element_type) rv = -1; \
  else if(a->element_type > b->element_type) rv = 1; \
  else rv = 0;
#define GET_TENSOR_TREE() \
  tensor_type_tree, DangValueTypeTensor *, GET_IS_RED, SET_IS_RED, \
  parent, left, right, COMPARE_TENSOR_TREE_NODES

/* TODO: optimize vector case */
static void
tensor_init_assign (DangValueType   *type,
                    void            *dst,
                    const void      *src)
{
  DangTensor **p_dst_tensor = (DangTensor **) dst;
  DangTensor *src_tensor = * (DangTensor **) src;
  DANG_UNUSED (type);
  if (src_tensor)
    src_tensor->ref_count += 1;
  *p_dst_tensor = src_tensor;
}
static void
tensor_destruct (DangValueType *type,
                 void          *data)
{
  DangTensor *tensor = * (DangTensor **) data;
  DangValueTypeTensor *ttype = (DangValueTypeTensor *) type;
  if (tensor == NULL)
    return;
  if (--(tensor->ref_count) > 0)
    return;
  if (ttype->element_type->destruct != NULL)
    {
      DangValueType *etype = ttype->element_type;
      unsigned i, N = 1;
      char *at = tensor->data;
      for (i = 0; i < ttype->rank; i++)
        N *= tensor->sizes[i];
      for (i = 0; i < N; i++)
        {
          etype->destruct (etype, at);
          at += etype->sizeof_instance;
        }
    }
  dang_free (tensor->data);
  dang_free (tensor);
}

static void
tensor_assign (DangValueType   *type,
               void            *dst,
               const void      *src)
{
  tensor_destruct (type, dst);
  tensor_init_assign (type, dst, src);
}


static void
append_tensor_to_string (DangValueType *elt_type,
                         unsigned rank,
                         const unsigned *sizes,
                         const void **value_inout,
                         DangStringBuffer *target)
{
  if (rank == 0)
    {
      const char *value = *value_inout;
      char *sub = dang_value_to_string (elt_type, value);
      dang_string_buffer_append (target, sub);
      dang_free (sub);
      *value_inout = value + elt_type->sizeof_instance;
    }
  else
    {
      unsigned i, N = *sizes;
      dang_string_buffer_append_c (target, '[');
      for (i = 0; i < N; i++)
        {
          if (i > 0)
            dang_string_buffer_append_c (target, ' ');
          append_tensor_to_string (elt_type, rank - 1, sizes + 1, value_inout, target);
        }
      dang_string_buffer_append_c (target, ']');
    }
}

static char *
tensor_to_string (DangValueType *type,
                  const void    *value)
{
  DangValueTypeTensor *ttype = (DangValueTypeTensor *) type;
  const DangTensor *tensor = * (const DangTensor *const *) value;
  if (tensor == NULL)
    {
      char *rv = dang_malloc (ttype->rank * 2 + 1);
      memset (rv, '[', ttype->rank);
      memset (rv + ttype->rank, ']', ttype->rank);
      rv[ttype->rank * 2] = 0;
      return rv;
    }
  else
    {
      const unsigned *sizes = tensor->sizes;
      const void *elements = tensor->data;
      DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
      append_tensor_to_string (ttype->element_type, ttype->rank, sizes, &elements, &buf);
      return buf.str;
    }
}

static void
oob_error (DangError **error,
           unsigned    which_index,
           unsigned    dim,
           unsigned    index)
{
  dang_set_error (error, "index #%u to tensor index is out-of-bounds: index %u with dimension %u",
                  which_index+1, index, dim);
}

static dang_boolean
index_get_ptr_tensor (DangValueIndexInfo *index_info,
                      void          *container,
                      const void   **indices,
                      void         **rv_ptr_out,
                      DangError    **error)
{
  DangTensor *tensor = container;
  DangValueTypeTensor *ttype = (DangValueTypeTensor *) index_info->owner;
  unsigned rank = ttype->rank;
  uint32_t ind = * (uint32_t*)(indices[0]);
  unsigned overall_ind, i;
  if (ind >= tensor->sizes[0])
    {
      oob_error (error, 0, tensor->sizes[0], ind);
      return FALSE;
    }
  overall_ind = ind;
  for (i = 1; i < rank; i++)
    {
      uint32_t ind = * (uint32_t*)(indices[i]);
      if (ind >= tensor->sizes[i])
        {
          oob_error (error, i, tensor->sizes[i], ind);
          return FALSE;
        }
      overall_ind *= tensor->sizes[i];
      overall_ind += ind;
    }
  *rv_ptr_out = (char*)tensor->data + overall_ind * ttype->element_type->sizeof_instance;
  return TRUE;
}
static dang_boolean
index_get__tensor   (DangValueIndexInfo *ii,
                     void          *container,
                     const void   **indices,
                     void          *rv_out,
                     dang_boolean   may_create,
                     DangError    **error)
{
  DangValueType *elt_type = ((DangValueTypeTensor*)ii->owner)->element_type;
  void *ptr;
  DANG_UNUSED (may_create);
  if (!index_get_ptr_tensor (ii, container, indices, &ptr, error))
    return FALSE;
  if (elt_type->init_assign)
    elt_type->init_assign (elt_type, rv_out, ptr);
  else
    memcpy (rv_out, ptr, elt_type->sizeof_instance);
  return TRUE;
}

static dang_boolean
index_set__tensor   (DangValueIndexInfo *ii,
                     void          *container,
                     const void   **indices,
                     const void    *element_value,
                     dang_boolean   may_create,
                     DangError    **error)
{
  DangValueType *elt_type = ((DangValueTypeTensor*)ii->owner)->element_type;
  void *ptr;
  DANG_UNUSED (may_create);
  if (!index_get_ptr_tensor (ii, container, indices, &ptr, error))
    return FALSE;
  if (elt_type->assign)
    elt_type->assign (elt_type, ptr, element_value);
  else
    memcpy (ptr, element_value, elt_type->sizeof_instance);
  return TRUE;
}

static DangValueType **
make_repeated_type (DangValueType **fill,
                    unsigned        N,
                    DangValueType  *s)
{
  unsigned i;
  for (i = 0; i < N; i++)
    fill[i] = s;
  return fill;
}

DangValueType *
dang_value_type_tensor (DangValueType *element_type,
                        unsigned       rank)
{
  DangValueTypeTensor dummy, *out, *conflict = NULL;
  static DangValueType *common_int32_array[MAX_STANDARD_RANK];
  static DangValueType *common_uint32_array[MAX_STANDARD_RANK];
  dummy.rank = rank;
  dummy.element_type = element_type;
  GSK_RBTREE_LOOKUP (GET_TENSOR_TREE (), &dummy, out);
  if (out != NULL)
    return (DangValueType *) out;
  if (tensor_type_tree == NULL)
    {
      make_repeated_type (common_int32_array, MAX_STANDARD_RANK, dang_value_type_int32());
      make_repeated_type (common_uint32_array, MAX_STANDARD_RANK, dang_value_type_uint32());
    }
  dang_assert (rank > 0);
  out = dang_new0 (DangValueTypeTensor, 1);
  out->base_type.magic = DANG_VALUE_TYPE_MAGIC;
  out->base_type.ref_count = 0;
  out->base_type.full_name = dang_strdup_printf ("tensor<%s,%u>", element_type->full_name, rank);

  out->base_type.sizeof_instance = DANG_SIZEOF_POINTER;
  out->base_type.alignof_instance = DANG_ALIGNOF_POINTER;
  
  out->base_type.init_assign = tensor_init_assign;
  out->base_type.assign = tensor_assign;
  out->base_type.destruct = tensor_destruct;
  out->base_type.to_string = tensor_to_string;
  out->base_type.cast_func_name = dang_strdup_printf ("operator_cast__tensor_%u__%s",
                                                      rank, element_type->full_name);
  out->base_type.internals.is_templated = element_type->internals.is_templated;
  out->element_type = element_type;
  out->rank = rank;
  out->base_type.internals.index_infos = out->index_infos;
  out->index_infos[0].next = &out->index_infos[1];
  out->index_infos[0].owner = &out->base_type;
  out->index_infos[0].n_indices = rank;
  if (rank > MAX_STANDARD_RANK)
    {
      out->index_infos[0].indices = dang_new (DangValueType *, rank);
      make_repeated_type (out->index_infos[0].indices, rank, dang_value_type_uint32 ());
      out->index_infos[1].indices = dang_new (DangValueType *, rank);
      make_repeated_type (out->index_infos[1].indices, rank, dang_value_type_int32 ());
    }
  else
    {
      out->index_infos[0].indices = common_uint32_array;
      out->index_infos[1].indices = common_int32_array;
    }
  out->index_infos[0].element_type = element_type;
  out->index_infos[0].get = index_get__tensor;
  out->index_infos[0].set = index_set__tensor;
  out->index_infos[0].element_type = element_type;
  out->index_infos[1].next = NULL;
  out->index_infos[1].owner = &out->base_type;
  out->index_infos[1].n_indices = rank;
  out->index_infos[1].element_type = element_type;
  out->index_infos[1].get = index_get__tensor;
  out->index_infos[1].set = index_set__tensor;
  out->index_infos[1].element_type = element_type;

  GSK_RBTREE_INSERT (GET_TENSOR_TREE (), out, conflict);
  dang_assert (conflict == NULL);
  return (DangValueType *) out;
}
dang_boolean dang_value_type_is_tensor (DangValueType *type)
{
  return type->to_string == tensor_to_string;
}

/* --- to_string --- */
typedef struct {
  DangFunction *elt_to_string;
  DangValueType *elt_type;
  //DangValueTypeTensor *tensor_type;
  unsigned rank;
} TensorToStringInfo;

static void
free_tensor_to_string_info (void *data)
{
  TensorToStringInfo *info = data;
  dang_function_unref (info->elt_to_string);
  dang_free (info);
}

static dang_boolean
print_tensor_recursive (unsigned rank,
                        const unsigned *dims,
                        TensorToStringInfo *info,
                        void **data_inout,
                        DangStringBuffer *buf_out,
                        DangError **error)
{
  if (rank == 0)
    {
      DangFunction *sub = info->elt_to_string;
      DangString *str = NULL;
      if (!dang_function_call_nonyielding_v (sub, &str, data_inout, error))
        return FALSE;
      if (str == NULL)
        dang_string_buffer_append (buf_out, "(null)");
      else
        {
          dang_string_buffer_append (buf_out, str->str);
          dang_string_unref (str);
        }
      *data_inout = (char*)(*data_inout) + info->elt_type->sizeof_instance;
    }
  else
    {
      unsigned i;
      dang_string_buffer_append_c (buf_out, '[');
      for (i = 0; i < *dims; i++)
        {
          if (i > 0)
            dang_string_buffer_append_c (buf_out, ' ');
          if (!print_tensor_recursive (rank - 1, dims + 1, info, data_inout, buf_out, error))
            return FALSE;
        }
      dang_string_buffer_append_c (buf_out, ']');
    }
  return TRUE;
}

static dang_boolean
to_string__tensor (void      **args,
                   void       *rv_out,
                   void       *func_data,
                   DangError **error)
{
  DangTensor *tensor = args[0];
  TensorToStringInfo *info = func_data;
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  void *data;

  data = tensor->data;
  if (!print_tensor_recursive (info->rank, tensor->sizes, info, &data, &buf, error))
    {
      dang_free (buf.str);
      return FALSE;
    }
  *(DangString**)rv_out = dang_string_new (buf.str);
  dang_free (buf.str);
  return TRUE;
}


/* --- box_form --- */

static dang_boolean
box_form__tensor (void      **args,
                   void       *rv_out,
                   void       *func_data,
                   DangError **error)
{
  DangTensor *tensor = args[0];
  TensorToStringInfo *info = func_data;
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  void *data = tensor->data;
  unsigned area = tensor->sizes[0] * tensor->sizes[1];
  DangString **strings = dang_new (DangString *, area);
  DangString **strings_at = strings;
  unsigned *col_widths = dang_new (unsigned, tensor->sizes[1]);
  unsigned r,c, i,j;

  for (i = 0; i < area; i++)
    {
      DangFunction *sub = info->elt_to_string;
      if (!dang_function_call_nonyielding_v (sub, strings_at, &data, error))
        {
          for (j = 0; j < i; j++)
            dang_string_unref (strings[j]);
          dang_free (strings);
          dang_free (col_widths);
          return FALSE;
        }
      data = (char*) data + info->elt_type->sizeof_instance;
      if (*strings_at == NULL)
        *strings_at = dang_string_new ("(null)");
      strings_at++;
    }
  for (c = 0; c < tensor->sizes[1]; c++)
    {
      unsigned width = 0;
      strings_at = strings + c;
      for (r = 0; r < tensor->sizes[0]; r++)
        {
          width = DANG_MAX (width, (*strings_at)->len);
          strings_at += tensor->sizes[1];
        }
      col_widths[c] = width;
    }
  strings_at = strings;
  for (r = 0; r < tensor->sizes[0]; r++)
    {
      for (c = 0; c < tensor->sizes[1]; c++)
        {
          unsigned pad = col_widths[c] - (*strings_at)->len;
          dang_string_buffer_append_repeated_char (&buf, ' ', pad / 2);
          dang_string_buffer_append_len (&buf, (*strings_at)->str, (*strings_at)->len);
          dang_string_buffer_append_repeated_char (&buf, ' ', (pad+1) / 2);
          dang_string_buffer_append_c (&buf, c+1 == tensor->sizes[1] ? '\n' : ' ');
          dang_string_unref (*strings_at);
          strings_at++;
        }
    }
  * (DangString **) rv_out = dang_string_new (buf.str);
  dang_free (buf.str);
  dang_free (strings);
  dang_free (col_widths);
  return TRUE;
}


/* --- Generic variadic_c function to deal with box_form or to_string --- */
typedef struct _VariadicSubstringsData VariadicSubstringsData;
struct _VariadicSubstringsData
{
  DangSimpleCFunc func;
  dang_boolean rank_2_only;
};


static DangFunction *
variadic_c__generic_substrings (DangMatchQuery *query,
                                 void *data,
                                 DangError **error)
{
  DangMatchQueryElement subquery_elt;
  DangMatchQuery subquery;
  DangValueTypeTensor *ttype;
  DangFunctionParam param;
  DangFunction *subfunc;
  DangSignature *sig;
  TensorToStringInfo *to_string_info;
  char *to_string_str = "to_string";
  VariadicSubstringsData *vsd = data;
  if (query->n_elements != 1
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || !dang_value_type_is_tensor (query->elements[0].info.simple_input))
    {
      return NULL;
    }
  ttype = (DangValueTypeTensor *) query->elements[0].info.simple_input;
  if (vsd->rank_2_only && ttype->rank != 2)
    {
      return NULL;
    }
  memset (&subquery_elt, 0, sizeof (subquery_elt));
  subquery_elt.type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
  subquery_elt.info.simple_input = ttype->element_type;
  subquery.n_elements = 1;
  subquery.elements = &subquery_elt;
  subquery.imports = query->imports;
  subfunc = dang_imports_lookup_function (query->imports,
                                 1, &to_string_str, &subquery,
                                 error);
  if (subfunc == NULL)
    return NULL;

  param.dir = DANG_FUNCTION_PARAM_IN;
  param.type = (DangValueType*) ttype;
  param.name = NULL;
  sig = dang_signature_new (dang_value_type_string (), 1, &param);
  to_string_info = dang_new (TensorToStringInfo, 1);
  to_string_info->elt_to_string = subfunc;
  to_string_info->elt_type = ttype->element_type;
  to_string_info->rank = ttype->rank;
  ttype->to_string_function
    = dang_function_new_simple_c (sig, vsd->func,
                                  to_string_info,
                                  free_tensor_to_string_info);
  dang_signature_unref (sig);
  return dang_function_ref (ttype->to_string_function);
}
typedef struct 
{
  DangFunction *subfunc;
  unsigned rank;
  DangValueType *element_type;
} ChainFuncData;
static void
free_chain_func_data (void *data)
{
  ChainFuncData *c = data;
  dang_function_unref (c->subfunc);
  dang_free (c);
}

DANG_SIMPLE_C_FUNC_DECLARE(do_tensor_operator_notequal)
{
  ChainFuncData *f = func_data;
  const DangTensor *a = args[0];
  const DangTensor *b = args[1];
  unsigned total_size = 1;
  void *values[2];
  unsigned i;
  DangFunction *sub = f->subfunc;
  unsigned elt_size = f->element_type->sizeof_instance;
  for (i = 0; i < f->rank; i++)
    {
      if (a->sizes[i] != b->sizes[i])
        {
          *(char*)rv_out = 1;
          return TRUE;
        }
      total_size *= a->sizes[i];
    }
  values[0] = a->data;
  values[1] = b->data;
  for (i = 0; i < total_size; i++)
    {
      char rv;
      if (!dang_function_call_nonyielding_v (sub, &rv, values, error))
        return FALSE;
      if (rv == 0)
        {
          *(char*)rv_out = 1;
          return TRUE;
        }
      values[0] = (char*)values[0] + elt_size;
      values[1] = (char*)values[1] + elt_size;
    }
  *(char*)rv_out = 0;
  return TRUE;
}

DANG_SIMPLE_C_FUNC_DECLARE(do_tensor_operator_equal)
{
  ChainFuncData *f = func_data;
  const DangTensor *a = args[0];
  const DangTensor *b = args[1];
  unsigned total_size = 1;
  void *values[2];
  unsigned i;
  DangFunction *sub = f->subfunc;
  unsigned elt_size = f->element_type->sizeof_instance;
  for (i = 0; i < f->rank; i++)
    {
      if (a->sizes[i] != b->sizes[i])
        {
          *(char*)rv_out = 0;
          return TRUE;
        }
      total_size *= a->sizes[i];
    }
  values[0] = a->data;
  values[1] = b->data;
  for (i = 0; i < total_size; i++)
    {
      char rv;
      if (!dang_function_call_nonyielding_v (sub, &rv, values, error))
        return FALSE;
      if (rv == 0)
        {
          *(char*)rv_out = 0;
          return TRUE;
        }
      values[0] = (char*)values[0] + elt_size;
      values[1] = (char*)values[1] + elt_size;
    }
  *(char*)rv_out = 1;
  return TRUE;
}

static DangFunction *
variadic_c__operator_equal (DangMatchQuery *query,
                            void *data,
                            DangError **error)
{
  DangValueTypeTensor *ttype;
  DangMatchQuery subquery;
  DangMatchQueryElement subelements[2];
  DangFunctionParam params[2];
  DangFunction *subfunc;
  char *operator_equal_str = "operator_equal";
  DangSignature *sig;
  ChainFuncData *func_data;
  DangFunction *rv;
  dang_boolean invert = (unsigned) data;
  if (query->n_elements != 2)
    return FALSE;
  if (query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || query->elements[1].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return FALSE;
  if (!dang_value_type_is_tensor (query->elements[0].info.simple_input)
   || query->elements[0].info.simple_input != query->elements[1].info.simple_input)
    return FALSE;
  ttype = (DangValueTypeTensor*) query->elements[0].info.simple_input;
  subquery.imports = query->imports;
  subquery.n_elements = 2;
  subquery.elements = subelements;
  subelements[0].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
  subelements[0].info.simple_input = ttype->element_type;
  subelements[1] = subelements[0];
  subfunc = dang_imports_lookup_function (query->imports,
                                 1, &operator_equal_str, &subquery,
                                 error);
  if (subfunc == NULL)
    return FALSE;
  params[0].name = NULL;
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].type = query->elements[0].info.simple_input;
  params[1] = params[0];
  sig = dang_signature_new (dang_value_type_boolean (), 2, params);
  func_data = dang_new (ChainFuncData, 1);
  func_data->subfunc = subfunc;
  func_data->rank = ttype->rank;
  func_data->element_type = ttype->element_type;
  rv = dang_function_new_simple_c (sig,
                                   invert ? do_tensor_operator_notequal : do_tensor_operator_equal,
                                   func_data, free_chain_func_data);
  dang_signature_unref (sig);
  return rv;
}

static DANG_SIMPLE_C_FUNC_DECLARE (do_tensor_dims)
{
  DangVector *rv = (DangVector *) rv_out;
  DangTensor *in = args[0];
  unsigned rank = (unsigned) func_data;
  uint32_t *arr;
  unsigned i;
  DANG_UNUSED (error);
  rv->len = rank;
  arr = dang_new (uint32_t, rank);
  rv->data = arr;
  for (i = 0; i < rank; i++)
    arr[i] = in->sizes[i];
  return TRUE;
}
  
static DangFunction *
variadic_c__dims (DangMatchQuery *query,
                  void *data,
                  DangError **error)
{
  DangFunctionParam params[1];
  DangFunction *rv;
  DangSignature *sig;
  unsigned rank;
  DANG_UNUSED (data);
  DANG_UNUSED (error);
  if (query->n_elements != 1)
    return FALSE;
  if (query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return FALSE;
  if (!dang_value_type_is_tensor (query->elements[0].info.simple_input))
    return FALSE;
  rank = ((DangValueTypeTensor*)query->elements[0].info.simple_input)->rank;
  params[0].name = NULL;
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].type = query->elements[0].info.simple_input;
  sig = dang_signature_new (dang_value_type_array (dang_value_type_uint32 ()),
                            1, params);
  rv = dang_function_new_simple_c (sig, do_tensor_dims, (void*)rank, NULL);
  dang_signature_unref (sig);
  return rv;
}

static DANG_SIMPLE_C_FUNC_DECLARE (do_vector_length)
{
  uint32_t *out = rv_out;
  DangVector *in = args[0];
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  *out = in->len;
  return TRUE;
}
  
static DangFunction *
try_sig__vector__length (DangMatchQuery *query,
                  void *data,
                  DangError **error)
{
  DangFunctionParam params[1];
  DangFunction *rv;
  DangSignature *sig;
  unsigned rank;
  DANG_UNUSED (data);
  DANG_UNUSED (error);
  if (query->n_elements != 1)
    return FALSE;
  if (query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return FALSE;
  if (!dang_value_type_is_tensor (query->elements[0].info.simple_input))
    return FALSE;
  rank = ((DangValueTypeTensor*)query->elements[0].info.simple_input)->rank;
  if (rank != 1)
    return FALSE;
  params[0].name = NULL;
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].type = query->elements[0].info.simple_input;
  sig = dang_signature_new (dang_value_type_uint32 (), 1, params);
  rv = dang_function_new_simple_c (sig, do_vector_length, NULL, NULL);
  dang_signature_unref (sig);
  return rv;
}

/* --- tensor_map() --- */
//static void
//handle_tensor_map  (DangExpr              *expr,
//                    DangBuilder   *builder,
//                    void                  *func_data,
//                    DangCompileFlags      *flags,
//                    DangCompileResult     *result)
//{
//  ...
//}


typedef struct _TensorMapData TensorMapData;
struct _TensorMapData
{
  unsigned n_inputs;
  DangValueType **input_types;          /* element types */
  DangValueType *output_type;           /* element type */
  unsigned rank;
};
static void
free_tensor_map_data (void *data)
{
  TensorMapData *md = data;
  dang_free (md->input_types);
  dang_free (md);
}

static dang_boolean
do_tensor_map (void      **args,
               void       *rv_out,
               void       *func_data,
               DangError **error)
{
  TensorMapData *md = func_data;
  unsigned n_inputs = md->n_inputs;
  char **at = dang_newa (char *, n_inputs);
  unsigned *elt_sizes = dang_newa (unsigned, n_inputs);
  DangFunction *func = *(DangFunction**)(args[n_inputs]);
  unsigned total_size = 1;
  unsigned i, j, d;
  DangTensor *rv;
  DangValueType *elt_type = md->output_type;
  char *rv_at;
  if (func == NULL)
    {
      dang_set_error (error, "null-pointer exception");
      return FALSE;
    }
  rv = rv_out;
  for (d = 0; d < md->rank; d++)
    {
      unsigned dim = ((DangTensor*)args[0])->sizes[d];
      total_size *= dim;
      for (i = 1; i < n_inputs; i++)
        {
          unsigned this_dim = ((DangTensor*)args[i])->sizes[d];
          if (dim != this_dim)
            {
              dang_set_error (error, "dimension #%u differ between params #1 and #%u to tensor.map", dim+1, i+1);
              return FALSE;
            }
        }
    }
  for (i = 0; i < n_inputs; i++)
    {
      at[i] = ((DangTensor*)args[i])->data;
      elt_sizes[i] = md->input_types[i]->sizeof_instance;
    }

  void *rv_data;
  if (elt_type->destruct)
    rv_data = dang_malloc0 (total_size * elt_type->sizeof_instance);
  else
    rv_data = dang_malloc (total_size * elt_type->sizeof_instance);
  rv_at = rv_data;

  /* TODO: implement a way to recycle a thread
   *       to call the same function twice! */

  for (i = 0; i < total_size; i++)
    {
      if (!dang_function_call_nonyielding_v (func, rv_at, (void**)at, error))
        {
          /* free partially constructed tensor */
          if (elt_type->destruct)
            {
              rv_at = rv_data;
              for (j = 0; j < i; j++)
                {
                  elt_type->destruct (elt_type, rv_at);
                  rv_at += elt_type->sizeof_instance;
                }
            }
          dang_free (rv_data);
          return FALSE;
        }
      for (j = 0; j < n_inputs; j++)
        at[j] += elt_sizes[j];
      rv_at += elt_type->sizeof_instance;
    }

  for (d = 0; d < md->rank; d++)
    rv->sizes[d] = ((DangTensor*)args[0])->sizes[d];
  rv->data = rv_data;

  return TRUE;
}

/* For a given query element that supposed to be a function
   in n_params inputs conforming to params,  see if the query
   element looks promising enough to go with it.

   If the argument is a well-typed function, then just do
   a standard type-check.

   If the argument is a untyped function, then make a stub-function
   from it.  The process of making the stub will cause that function
   to get annotated, so that we can figure out its return value,
   or give a compile error if the types don't work out.

   NOTE: The pointer placed in *sig_out does hold a reference.
 */
static dang_boolean
dang_match_function_from_params (DangMatchQueryElement *elt,
                                 unsigned               n_params,
                                 DangFunctionParam     *params,
                                 const char            *name,
                                 DangSignature        **sig_out,
                                 DangError            **error)
{
  if (elt->type == DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   && dang_value_type_is_function (elt->info.simple_input))
    {
      DangSignature *func_sig;
      unsigned i;
      func_sig = ((DangValueTypeFunction*)elt->info.simple_input)->sig;
      if (func_sig->n_params != n_params)
        {
          dang_set_error (error, "argument to %s() had %u parameters, expected %u",
                          name, func_sig->n_params, n_params);
          return FALSE;
        }

      /* Ensure the parameters match. */
      for (i = 0; i < func_sig->n_params; i++)
        {
          if (func_sig->params[i].dir != DANG_FUNCTION_PARAM_IN)
            {
              dang_set_error (error, "argument to %s() itself took a non-input param, not allowed", name);
              return FALSE;
            }
          if (!dang_value_type_is_autocast (func_sig->params[i].type,
                                            params[i].type))
            {
              dang_set_error (error, "argument to %s() itself took a %s as its #%u argument, expected %s",
                              name, func_sig->params[i].type->full_name, i+1,
                              params[i].type->full_name);
              return FALSE;
            }
        }
      *sig_out = dang_signature_ref (func_sig);
    }
  else if (elt->type == DANG_MATCH_QUERY_ELEMENT_UNTYPED_FUNCTION)
    {
      DangUntypedFunction *untyped = elt->info.untyped_function;
      if (untyped->n_params != n_params)
        {
          dang_set_error (error, "argument to %s() had %u parameters, expected %u",
                          name, untyped->n_params, n_params);
          return FALSE;
        }

      if (!dang_untyped_function_make_stub (untyped, params, error))
        return FALSE;

      *sig_out = dang_signature_ref (elt->info.untyped_function->func->base.sig);
    }
  else if (elt->type == DANG_MATCH_QUERY_ELEMENT_FUNCTION_FAMILY)
    {
      DangFunctionFamily *ff = elt->info.function_family;
      DangMatchQueryElement *param_elts = dang_newa (DangMatchQueryElement, n_params);
      DangMatchQuery param_query;
      unsigned i;
      DangFunction *func;
      param_query.n_elements = n_params;
      param_query.elements = param_elts;
      param_query.imports = NULL;               /* maybe ok ??? */
      for (i = 0; i < n_params; i++)
        if (params[i].dir == DANG_FUNCTION_PARAM_IN)
          {
            param_elts[i].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
            param_elts[i].info.simple_input = params[i].type;
          }
        else
          {
            param_elts[i].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT;
            param_elts[i].info.simple_output = params[i].type;
          }
      func = dang_function_family_try (ff, &param_query, error);
      if (func == NULL)
        return FALSE;
      *sig_out = dang_signature_ref (func->base.sig);
      dang_function_unref (func);
    }
  else
    {
      dang_set_error (error, "argument to %s() not a function", name);
      return FALSE;
    }
  return TRUE;
}


/* Take an array of tensors of identical rank,
   and apply a function to them, returning a tensor of results
   (or error if they are not the same sizes). */
static DangFunction *
try_sig__tensor__map (DangMatchQuery *query, void *data, DangError **error)
{
  unsigned rank = 0;
  DangFunctionParam *fparams;
  unsigned i;
  DangSignature *func_sig;
  DangSignature *sig;           /* signature of this flavor of map() */
  DangFunction *rv;
  unsigned n_tensors = query->n_elements - 1;
  DANG_UNUSED (data);
  if (query->n_elements < 2)
    {
      dang_set_error (error, "expected at least two arguments to map()");
      return NULL;
    }
  fparams = dang_newa (DangFunctionParam, query->n_elements);
  for (i = 0; i < n_tensors; i++)
    {
      DangValueType *type;
      unsigned this_rank;
      if (query->elements[i].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
        {
          dang_set_error (error, "all args but last to map() must be simple input parameters");
          return NULL;
        }
      type = query->elements[i].info.simple_input;
      if (!dang_value_type_is_tensor (type))
        {
          dang_set_error (error, "all args but last to map() must be tensor");
          return NULL;
        }
      this_rank = ((DangValueTypeTensor*)type)->rank;
      if (i == 0)
        rank = this_rank;
      else if (rank != this_rank)
        {
          dang_set_error (error, "rank mismatch in map() (%u v %u)",
                          rank, this_rank);
          return NULL;
        }
      fparams[i].type = ((DangValueTypeTensor*)query->elements[i].info.simple_input)->element_type;
      fparams[i].dir = DANG_FUNCTION_PARAM_IN;
      fparams[i].name = NULL;
    }

  if (!dang_match_function_from_params (query->elements + n_tensors,
                                        n_tensors, fparams,
                                        "map", &func_sig, error))
    {
      return NULL;
    }

  /* Construct the tensor_map_data */
  TensorMapData *tensor_map_data;
  tensor_map_data = dang_new (TensorMapData, 1);
  tensor_map_data->n_inputs = n_tensors;
  tensor_map_data->input_types = dang_new (DangValueType *, tensor_map_data->n_inputs);
  for (i = 0; i < tensor_map_data->n_inputs; i++)
    tensor_map_data->input_types[i] = fparams[i].type;
  tensor_map_data->output_type = func_sig->return_type;
  tensor_map_data->rank = rank;

  /* Construct the overall signature of this flavor of map. */
  fparams[query->n_elements - 1].dir = DANG_FUNCTION_PARAM_IN;
  fparams[query->n_elements - 1].name = NULL;
  fparams[query->n_elements - 1].type = dang_value_type_function (func_sig);
  sig = dang_signature_new (dang_value_type_tensor (func_sig->return_type, rank),
                            query->n_elements, fparams);
  rv = dang_function_new_simple_c (sig, do_tensor_map,
                                   tensor_map_data,
                                   free_tensor_map_data);
  dang_signature_unref (sig);
  dang_signature_unref (func_sig);
  return rv;
}

typedef struct _NewTensorData NewTensorData;
struct _NewTensorData
{
  unsigned rank;
  DangValueType *elt_type;
};

static dang_boolean
do_new_tensor (void      **args,
               void       *rv_out,
               void       *func_data,
               DangError **error)
{
  NewTensorData *ntd = func_data;
  unsigned *dims = dang_newa (unsigned, ntd->rank);
  unsigned *indices = dang_newa (unsigned, ntd->rank);
  void **callee_args = dang_newa (void *, ntd->rank);
  DangTensor *tensor = dang_malloc (DANG_TENSOR_SIZEOF (ntd->rank));
  unsigned i;
  unsigned total_elements = 1;
  char *data, *data_at;
  DangFunction *func;
  tensor->ref_count = 1;
  for (i = 0; i < ntd->rank; i++)
    {
      indices[i] = 0;
      dims[i] = * (uint32_t*) args[i];
      callee_args[i] = indices + i;
      tensor->sizes[i] = dims[i];
      total_elements *= dims[i];
    }
  func = * (DangFunction **) args[ntd->rank];
  if (func == NULL)
    {
      dang_set_error (error, "null-pointer exception");
      dang_free (tensor);
      return FALSE;
    }
  if (ntd->elt_type->init_assign)
    data = dang_malloc0 (total_elements * ntd->elt_type->sizeof_instance);
  else
    data = dang_malloc (total_elements * ntd->elt_type->sizeof_instance);

  data_at = data;
  if (total_elements)
  for (;;)
    {
      /* eval element */
      if (!dang_function_call_nonyielding_v (func, data_at, callee_args, error))
        {
          if (ntd->elt_type->destruct)
            {
              char *sub_at = data;
              while (sub_at < data_at)
                {
                  ntd->elt_type->destruct (ntd->elt_type, sub_at);
                  sub_at += ntd->elt_type->sizeof_instance;
                }
            }
          dang_free (data);
          memset (tensor, 0, DANG_TENSOR_SIZEOF (ntd->rank));
          dang_free (tensor);
          return FALSE;
        }

      i = ntd->rank - 1;
      while (++indices[i] == dims[i])
        {
          indices[i] = 0;
          if (i == 0)
            goto done;
          i--;
        }
      data_at += ntd->elt_type->sizeof_instance;
    }
done:

  tensor->data = data;
  *(DangTensor**)rv_out = tensor;
  return TRUE;
}

static DangFunction *
try_sig__tensor__new_tensor (DangMatchQuery *query,
                             void *data,
                             DangError **error)
{
  unsigned i;
  unsigned rank = query->n_elements - 1;
  DangSignature *subsig, *sig;
  DangValueType *elt_type = NULL;
  DangFunctionParam *params;
  DangValueType *fcttype;
  DangValueType *ret_type;
  NewTensorData *new_tensor_data;
  DangFunction *rv;
  DangMatchQueryElement *elt;
  DANG_UNUSED (data);
  if (query->n_elements < 2)
    {
      dang_set_error (error, "new_tensor requires at least two args");
      return NULL;
    }
  params = dang_newa (DangFunctionParam, query->n_elements);
  for (i = 0; i < query->n_elements - 1; i++)
    {
      params[i].dir = DANG_FUNCTION_PARAM_IN;
      params[i].type = dang_value_type_uint32 ();
      params[i].name = NULL;
    }
  for (i = 0; i < query->n_elements - 1; i++)
    {
      elt = query->elements + i;
      if (elt->type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
        || elt->info.simple_input != dang_value_type_uint32 ())
        {
          dang_set_error (error, "new_tensor got non-uint type %s to arg #%u",
                          elt->info.simple_input->full_name, i+1);
          return NULL;
        }
    }
  elt = query->elements + i;
  if (elt->type == DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    {
      DangValueTypeFunction *fcttype;
      if (!dang_value_type_is_function (elt->info.simple_input))
        {
          dang_set_error (error, "expected function for last arg of new_tensor, got %s",
                          elt->info.simple_input->full_name);
          return NULL;
        }

      /* verify its prototype */
      fcttype = (DangValueTypeFunction *) elt->info.simple_input;
      if (fcttype->sig->n_params != query->n_elements - 1)
        {
          dang_set_error (error, "expected function to take %u args, it took %u", query->n_elements - 1, fcttype->sig->n_params);
          return NULL;
        }
      for (i = 0; i < query->n_elements - 1; i++)
        {
          if (fcttype->sig->params[i].dir != DANG_FUNCTION_PARAM_IN
           || fcttype->sig->params[i].type != dang_value_type_uint32 ())
            {
              dang_set_error (error, "last arg of new_tensor should take only ints as params, got %s for arg #%u",
                              fcttype->sig->params[i].type->full_name, i+1);
              return NULL;
            }
        }

      elt_type = fcttype->sig->return_type;
    }
  else if (elt->type == DANG_MATCH_QUERY_ELEMENT_UNTYPED_FUNCTION)
    {
      /* Compute the parameters. */
      if (!dang_untyped_function_make_stub (elt->info.untyped_function,
                                            params, error))
        return FALSE;
      elt_type = elt->info.untyped_function->func->base.sig->return_type;
    }
  else
    {
      dang_set_error (error, "last arg of new-tensor was %s, not allowed",
                      dang_match_query_element_type_name (elt->type));
      return NULL;
    }

  subsig = dang_signature_new (elt_type, query->n_elements - 1, params);
  fcttype = dang_value_type_function (subsig);
  dang_signature_unref (subsig);
  params[i].dir = DANG_FUNCTION_PARAM_IN;
  params[i].type = fcttype;
  params[i].name = NULL;
  ret_type = dang_value_type_tensor (elt_type, rank);
  sig = dang_signature_new (ret_type, query->n_elements, params);
  new_tensor_data = dang_new (NewTensorData, 1);
  new_tensor_data->rank = rank;
  new_tensor_data->elt_type = elt_type;
  rv = dang_function_new_simple_c (sig, do_new_tensor,
                                   new_tensor_data,
                                   dang_free);
  dang_signature_unref (sig);
  return rv;
}


static dang_boolean
do_grep_vector (void      **args,
               void       *rv_out,
               void       *func_data,
               DangError **error)
{
  DangValueType *elt_type = func_data;
  unsigned elt_size = elt_type->sizeof_instance;
  DangVector *vector = args[0];
  DangFunction *func = *(DangFunction**)(args[1]);
  DangVector *rv = rv_out;
  unsigned bitvec_size;
  void *to_free = NULL;
  uint8_t *bitvec, *bitvec_at;
  uint8_t mask;
  unsigned i;
  uint8_t *call_rv_buf;
  unsigned grepfunc_rv_size = func->base.sig->return_type->sizeof_instance;
  unsigned new_len;
  if (func == NULL)
    {
      dang_set_error (error, "null-pointer exception");
      return FALSE;
    }
  bitvec_size = (vector->len + 7) / 8;
  if (bitvec_size < 1024)
    bitvec = dang_alloca (bitvec_size);
  else
    to_free = bitvec = dang_malloc (bitvec_size);
  memset (bitvec, 0, bitvec_size);

  call_rv_buf = dang_alloca (grepfunc_rv_size);

  /* TODO: implement a way to recycle a thread
   *       to call the same function twice! */

  bitvec_at = bitvec;
  mask = 1;
  new_len = 0;
  char *input_at = vector->data;
  if (grepfunc_rv_size == 1)
    for (i = 0; i < vector->len; i++)
      {
        if (!dang_function_call_nonyielding_v (func, call_rv_buf, (void**) &input_at, error))
          goto call_failed;
        if (call_rv_buf[0] != 0)
          {
            *bitvec_at |= mask;
            new_len++;
          }
        mask <<= 1;
        if (mask == 0)
          {
            mask = 1;
            bitvec_at++;
          }
        input_at += elt_size;
      }
  else
    for (i = 0; i < vector->len; i++)
      {
        if (!dang_function_call_nonyielding_v (func, call_rv_buf, (void**) &input_at, error))
          goto call_failed;
        if (!dang_util_is_zero (call_rv_buf, grepfunc_rv_size))
          {
            *bitvec_at |= mask;
            new_len++;
          }
        mask <<= 1;
        if (mask == 0)
          {
            mask = 1;
            bitvec_at++;
          }
        input_at += elt_size;
      }

  rv->len = new_len;
  rv->data = dang_malloc (elt_size * new_len);
  bitvec_at = bitvec;
  char *output_at = rv->data;
  input_at = vector->data;
  mask = 1;
  if (elt_type->init_assign)
    {
      for (i = 0; i < vector->len; i++)
        {
          if (*bitvec_at & mask)
            {
              elt_type->init_assign (elt_type, output_at, input_at);
              output_at += elt_size;
            }
          mask <<= 1;
          if (mask == 0)
            {
              mask = 1;
              bitvec_at++;
            }
          input_at += elt_size;
        }
    }
  else
    {
      /* TODO: optimize for *bitvec_at == 0 and *bitvec_at == 255. */
      for (i = 0; i < vector->len; i++)
        {
          if (*bitvec_at & mask)
            {
              memcpy (output_at, input_at, elt_size);
              output_at += elt_size;
            }
          mask <<= 1;
          if (mask == 0)
            {
              mask = 1;
              bitvec_at++;
            }
          input_at += elt_size;
        }
    }
  if (to_free)
    dang_free (to_free);

  return TRUE;

call_failed:
  dang_free (to_free);
  return FALSE;
}

/* grep(vector<A>, function<A : boolean> : vector<A>) */
static DangFunction *
try_sig__vector__grep       (DangMatchQuery *query,
                             void *data,
                             DangError **error)
{
  DangFunctionParam params[2];
  DangValueTypeTensor *ttype;
  DangSignature *func_sig;
  DangSignature *sig;
  DANG_UNUSED (data);
  if (query->n_elements != 2)
    {
      dang_set_error (error, "grep(vector, function) requires two arguments");
      return NULL;
    }
  if (query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || !dang_value_type_is_tensor (query->elements[0].info.simple_input)
   || (ttype=(DangValueTypeTensor*)query->elements[0].info.simple_input) == NULL
   || ttype->rank != 1)
    {
      dang_set_error (error, "first argument to 'grep' must a vector");
      return NULL;
    }

  params[0].name = NULL;
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].type = ttype->element_type;

  if (!dang_match_function_from_params (query->elements + 1,
                                        1, params, "grep", &func_sig, error))
    return NULL;

  if (func_sig->return_type == NULL
   || func_sig->return_type == dang_value_type_void ())
    {
      dang_set_error (error, "function argument to grep must not return void");
      dang_signature_unref (func_sig);
      return NULL;
    }
  if (func_sig->return_type->destruct != NULL
   || func_sig->return_type->sizeof_instance > 1024)
    {
      dang_set_error (error, "function argument to grep's return-type too complex or big (type=%s)",
                      func_sig->return_type->full_name);
      dang_signature_unref (func_sig);
      return NULL;
    }

  /* OK, now build a signature for our 'grep' variant that takes
     the exact right sig: grep(vector<A>, function<A : B> : vector<A>)
     where A is the element type and B is the return type 
     of the second param. */
  params[0].type = (DangValueType*) ttype;
  params[1].name = NULL;
  params[1].dir = DANG_FUNCTION_PARAM_IN;
  params[1].type = dang_value_type_function (func_sig);
  sig = dang_signature_new ((DangValueType *) ttype, 2, params);

  DangFunction *rv;
  rv = dang_function_new_simple_c (sig, do_grep_vector, ttype->element_type, NULL);
  dang_signature_unref (sig);
  dang_signature_unref (func_sig);
  return rv;
}
  /* Take two tensors of the same rank and element type,
     and, if we know about the element-type,
     bulk perform that operation.
   */
typedef struct {
  DangValueType *type;
  void (*op) (const void *a,
              const void *b,
              void       *dst,
              unsigned    N);
} BulkOpTableEntry;

typedef struct {
  BulkOpTableEntry *op_info;
  unsigned rank;
  const char *op_name;                  /* static string */
} ElementwiseOpFuncInfo;



static DANG_SIMPLE_C_FUNC_DECLARE (do_elementwise_op)
{
  ElementwiseOpFuncInfo *fi = func_data;
  unsigned rank = fi->rank;
  DangTensor *a = args[0];
  DangTensor *b = args[1];
  DangTensor *c = rv_out;
  unsigned total_elements;
  unsigned i;
  dang_assert (rank > 0);
  total_elements = a->sizes[0];
  if (b->sizes[0] != a->sizes[0])
    {
      i = 0;
      goto size_mismatch;
      return FALSE;
    }
  for (i = 1; i < rank; i++)
    {
      if (a->sizes[i] != b->sizes[i])
        goto size_mismatch;
      total_elements *= a->sizes[i];
    }
  c->data = dang_malloc (total_elements * fi->op_info->type->sizeof_instance);
  for (i = 0; i < rank; i++)
    c->sizes[i] = a->sizes[i];
  fi->op_info->op (a->data, b->data, c->data, total_elements);
  return TRUE;

size_mismatch:
  if (rank == 1)
    {
      dang_set_error (error, "vector arguments to %s differ in size (%u v %u)",
                      fi->op_name, a->sizes[0], b->sizes[0]);
      return FALSE;
    }
  else if (rank == 2)
    {
      dang_set_error (error, "matrix arguments to %s differ in size (%ux%u v %ux%u)",
                      fi->op_name,
                      a->sizes[0], a->sizes[1],
                      b->sizes[0], b->sizes[1]);
      return FALSE;
    }
  else
    {
      dang_set_error (error, "tensor arguments to %s differ in size in index %u: %u v %u",
                      fi->op_name,
                      i, a->sizes[i], b->sizes[i]);
      return FALSE;
    }
}

static DangFunction *
try_sig__tensor__bulk_op (DangMatchQuery *query,
                          unsigned n_table_entries,
                          BulkOpTableEntry *table_entries,
                          const char *op_name,
                          DangError **error)
{
  DangFunction *rv;
  DangFunctionParam params[2];
  DangSignature *sig;
  ElementwiseOpFuncInfo *func_data;
  DangValueTypeTensor *ttype;
  DangValueType *elt_type;
  unsigned i;
  if (query->n_elements != 2
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || query->elements[1].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return FALSE;
  if (!dang_value_type_is_tensor (query->elements[0].info.simple_input)
   && !dang_value_type_is_tensor (query->elements[1].info.simple_input))
    return FALSE;

  /* at least one is a tensor, so an error is a real error */
  if (query->elements[0].info.simple_input
      != query->elements[1].info.simple_input)
    {
      dang_set_error (error, "type mismatch in %s: %s v %s",
                      op_name,
                      query->elements[0].info.simple_input->full_name,
                      query->elements[1].info.simple_input->full_name);
      return NULL;
    }

  ttype = (DangValueTypeTensor*)query->elements[0].info.simple_input;
  elt_type = ttype->element_type;
  for (i = 0; i < n_table_entries; i++)
    if (table_entries[i].type == elt_type)
      break;
  if (i == n_table_entries)
    return NULL;

  params[0].type = query->elements[0].info.simple_input;
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].name = NULL;
  params[1] = params[0];
  sig = dang_signature_new (params[0].type, 2, params);

  /* Create new function. */
  func_data = dang_new (ElementwiseOpFuncInfo, 1);
  func_data->op_info = table_entries + i;
  func_data->rank = ttype->rank;
  func_data->op_name = op_name;
  rv = dang_function_new_simple_c (sig, do_elementwise_op, func_data, dang_free);

  dang_signature_unref (sig);

  return rv;
}

#define DEFINE_DO_BULK_OP(func_name, type, op_macro)                      \
static void func_name (const void *a, const void *b, void *c, unsigned N) \
{                                                                         \
  const type *a_at = a;                                                   \
  const type *b_at = b;                                                   \
  type *c_at = c;                                                         \
  while (N--)                                                             \
    {                                                                     \
      *c_at = op_macro (*a_at, *b_at);                                    \
      a_at++;                                                             \
      b_at++;                                                             \
      c_at++;                                                             \
    }                                                                     \
}

static BulkOpTableEntry add__op_table[3];

#define ADD_MACRO(a,b)  ((a) + (b))
DEFINE_DO_BULK_OP(add_bulk__int32, int32_t, ADD_MACRO)
DEFINE_DO_BULK_OP(add_bulk__float, float, ADD_MACRO)
DEFINE_DO_BULK_OP(add_bulk__double, double, ADD_MACRO)

static DangFunction *
try_sig__tensor__operator_add       (DangMatchQuery *query,
                                     void *data,
                                     DangError **error)
{
  DANG_UNUSED (data);
  if (add__op_table[0].type == NULL)
    {
      add__op_table[0].type = dang_value_type_int32 ();
      add__op_table[0].op = add_bulk__int32;
      add__op_table[1].type = dang_value_type_float ();
      add__op_table[1].op = add_bulk__float;
      add__op_table[2].type = dang_value_type_double ();
      add__op_table[2].op = add_bulk__double;
    }
  return try_sig__tensor__bulk_op (query,
                                   DANG_N_ELEMENTS (add__op_table),
                                   add__op_table,
                                   "+", error);
}

#define SUBTRACT_MACRO(a,b)  ((a) - (b))
DEFINE_DO_BULK_OP(subtract_bulk__int32, int32_t, SUBTRACT_MACRO)
DEFINE_DO_BULK_OP(subtract_bulk__float, float, SUBTRACT_MACRO)
DEFINE_DO_BULK_OP(subtract_bulk__double, double, SUBTRACT_MACRO)

static BulkOpTableEntry subtract__op_table[3];
static DangFunction *
try_sig__tensor__operator_subtract  (DangMatchQuery *query,
                                     void *data,
                                     DangError **error)
{
  DANG_UNUSED (data);
  if (subtract__op_table[0].type == NULL)
    {
      subtract__op_table[0].type = dang_value_type_int32 ();
      subtract__op_table[0].op = subtract_bulk__int32;
      subtract__op_table[1].type = dang_value_type_float ();
      subtract__op_table[1].op = subtract_bulk__float;
      subtract__op_table[2].type = dang_value_type_double ();
      subtract__op_table[2].op = subtract_bulk__double;
    }
  return try_sig__tensor__bulk_op (query,
                                   DANG_N_ELEMENTS (subtract__op_table),
                                   subtract__op_table,
                                   "-", error);
}

typedef void (*ScalarMultiplyFunc) (const void *tensor_data,
                                    const void *scalar,
                                    void *output,
                                    unsigned N);

typedef struct _ScalarMultiplyInfo ScalarMultiplyInfo;
struct _ScalarMultiplyInfo
{
  dang_boolean tensor_first;
  unsigned rank;
  unsigned elt_size;
  ScalarMultiplyFunc op;
};

#define DEFINE_DO_SCALAR_MULTIPLY(func_name, type)              \
static void func_name(const void *tensor_data,                  \
                      const void *scalar,                       \
                      void *output,                             \
                      unsigned N)                               \
{                                                               \
  const type *in = tensor_data;                                 \
  type *out = output;                                           \
  type s = *(const type*)scalar;                                \
  while (N--)                                                   \
    {                                                           \
      *out = *in * s;                                           \
      out++;                                                    \
      in++;                                                     \
    }                                                           \
}
DEFINE_DO_SCALAR_MULTIPLY(scalar_multiply__int32, int32_t)
DEFINE_DO_SCALAR_MULTIPLY(scalar_multiply__float, float)
DEFINE_DO_SCALAR_MULTIPLY(scalar_multiply__double, double)

static DANG_SIMPLE_C_FUNC_DECLARE (do_scalar_multiply)
{
  ScalarMultiplyInfo *smi = func_data;
  DangTensor *a = smi->tensor_first ? args[0] : args[1];
  const void *scalar = smi->tensor_first ? args[1] : args[0];
  DangTensor *out = rv_out;
  unsigned total_size = a->sizes[0];
  unsigned i;
  DANG_UNUSED (error);
  out->sizes[0] = a->sizes[0];
  for (i = 1; i < smi->rank; i++)
    {
      total_size *= a->sizes[i];
      out->sizes[i] = a->sizes[i];
    }
  out->data = dang_malloc (total_size * smi->elt_size);
  smi->op (a->data, scalar, out->data, total_size);
  return TRUE;
}

static DangFunction *
try_sig__tensor_scalar_multiply  (DangMatchQuery *query,
                                  void *data,
                                  DangError **error)
{
  DangValueType *ta, *tb, *elt_type;
  dang_boolean tensor_first;
  unsigned rank;
  DangFunction *rv;
  DangSignature *sig;
  ScalarMultiplyInfo *func_data;
  ScalarMultiplyFunc func;
  DANG_UNUSED (data);
  DANG_UNUSED (error);
  DangFunctionParam params[2];
  if (query->n_elements != 2
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || query->elements[1].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return FALSE;
  if (!dang_value_type_is_tensor (query->elements[0].info.simple_input)
   && !dang_value_type_is_tensor (query->elements[1].info.simple_input))
    return FALSE;
  ta = query->elements[0].info.simple_input;
  tb = query->elements[1].info.simple_input;
  if (dang_value_type_is_tensor (ta))
    {
      if (((DangValueTypeTensor*)ta)->element_type == tb)
        {
          elt_type = tb;
          tensor_first = TRUE;
          rank = ((DangValueTypeTensor*)ta)->rank;
          goto possible_match;
        }
    }
  if (dang_value_type_is_tensor (tb))
    {
      if (((DangValueTypeTensor*)tb)->element_type == ta)
        {
          elt_type = ta;
          tensor_first = FALSE;
          rank = ((DangValueTypeTensor*)tb)->rank;
          goto possible_match;
        }
    }
  return FALSE;

possible_match:
  if (elt_type == dang_value_type_int32 ())
    func = scalar_multiply__int32;
  else if (elt_type == dang_value_type_float ())
    func = scalar_multiply__float;
  else if (elt_type == dang_value_type_double ())
    func = scalar_multiply__double;
  else
    return FALSE;

  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].name = NULL;
  params[0].type = ta;
  params[1].dir = DANG_FUNCTION_PARAM_IN;
  params[1].name = NULL;
  params[1].type = tb;
  sig = dang_signature_new (tensor_first ? ta : tb, 2, params);

  func_data = dang_new (ScalarMultiplyInfo, 1);
  func_data->op = func;
  func_data->tensor_first = tensor_first;
  func_data->elt_size = elt_type->sizeof_instance;
  func_data->rank = rank;
  rv = dang_function_new_simple_c (sig, do_scalar_multiply, func_data, dang_free);
  dang_signature_unref (sig);
  return rv;
}

typedef struct _StatInfo StatInfo;
typedef struct _StatTypeInfo StatTypeInfo;
struct _StatTypeInfo
{
  DangValueType *element_type;
  void (*compute_stat) (void *out,
                        const void *in,
                        unsigned N);
  dang_boolean permits_0_len;
};
struct _StatInfo
{
  const char *name;
  unsigned n_type_infos;
  StatTypeInfo *type_infos;
};

typedef struct _StatFuncInfo StatFuncInfo;
struct _StatFuncInfo
{
  StatTypeInfo *type_info;
  unsigned rank;
};

static DANG_SIMPLE_C_FUNC_DECLARE (do_statistic)
{
  DangTensor *tensor = args[0];
  StatFuncInfo *sfi = func_data;
  unsigned rank = sfi->rank;
  unsigned N = tensor->sizes[0];
  unsigned i;
  DANG_UNUSED (error);
  for (i = 1; i < rank; i++)
    N *= tensor->sizes[i];
  if (N == 0 && !sfi->type_info->permits_0_len)
    {
      dang_set_error (error, "0-length tensor not allowed for statistic");
      return FALSE;
    }
  sfi->type_info->compute_stat (rv_out, tensor->data, N);
  return TRUE;
}

static DangFunction *
try_sig__tensor__statistic  (DangMatchQuery *query,
                                  void *data,
                                  DangError **error)
{
  StatInfo *stat = data;
  DangValueType *type;
  DangValueTypeTensor *ttype;
  DangValueType *elt_type;
  unsigned i;
  DANG_UNUSED (error);
  if (query->n_elements != 1
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return NULL;
  type = query->elements[0].info.simple_input;
  if (!dang_value_type_is_tensor (type))
    return NULL;
  ttype = (DangValueTypeTensor*) type;
  elt_type = ttype->element_type;
  for (i = 0; i < stat->n_type_infos; i++)
    if (stat->type_infos[i].element_type == elt_type)
      {
        DangFunctionParam param;
        DangSignature *sig;
        DangFunction *rv;
        StatFuncInfo *sfi;
        param.type = type;
        param.dir = DANG_FUNCTION_PARAM_IN;
        param.name = NULL;
        sig = dang_signature_new (elt_type, 1, &param);
        sfi = dang_new (StatFuncInfo, 1);
        sfi->type_info = stat->type_infos + i;
        sfi->rank = ttype->rank;
        rv = dang_function_new_simple_c (sig, do_statistic, sfi, dang_free);
        dang_signature_unref (sig);
        return rv;
      }
  return NULL;
}

#define DEFINE_EXTREME(type, ctype, name, cmp)                     \
static void name##__##type (void *out, const void *in, unsigned N) \
{                                                                  \
  const ctype *a = in;                                             \
  ctype v = *a++;                                                  \
  N--;                                                             \
  while (N--)                                                      \
    {                                                              \
      if (*a cmp v)                                                \
        v = *a;                                                    \
      a++;                                                         \
    }                                                              \
  *(ctype*)out = v;                                                \
}
#define DEFINE_MIN(type, ctype) DEFINE_EXTREME(type, ctype, min, <)
#define DEFINE_MAX(type, ctype) DEFINE_EXTREME(type, ctype, max, >)
#define DEFINE_FOLD(type, ctype, name, start_val, op)              \
static void name##__##type (void *out, const void *in, unsigned N) \
{                                                                  \
  const ctype *a = in;                                             \
  ctype v = start_val;                                             \
  while (N--)                                                      \
    {                                                              \
      v op *a;                                                     \
      a++;                                                         \
    }                                                              \
  *(ctype*)out = v;                                                \
}
#define DEFINE_SUM(type, ctype) DEFINE_FOLD(type, ctype, sum, 0, +=)
#define DEFINE_PRODUCT(type, ctype) DEFINE_FOLD(type, ctype, product, 1, *=)
#define DEFINE_AVERAGE(type, ctype)                          \
static void average__##type (void *out, const void *in, unsigned N) \
{                                                                  \
  ctype *o = out;                                                  \
  sum__##type (out, in, N);                                        \
  *o /= N;                                                         \
}

#define DEFINE_ALL_STATISTICS(type, ctype) \
  DEFINE_MIN(type, ctype) \
  DEFINE_MAX(type, ctype) \
  DEFINE_SUM(type, ctype) \
  DEFINE_PRODUCT(type, ctype) \
  DEFINE_AVERAGE(type, ctype)

DEFINE_ALL_STATISTICS(int32, int32_t)
DEFINE_ALL_STATISTICS(uint32, uint32_t)
DEFINE_ALL_STATISTICS(float, float)
DEFINE_ALL_STATISTICS(double, double)
  
/* transpose() */

static DANG_SIMPLE_C_FUNC_DECLARE (do_transpose__memcpy4)
{
  DangMatrix *in = * (DangMatrix **) args[0];
  DangMatrix *out;
  unsigned src_rows = in->n_rows;
  unsigned src_cols = in->n_cols;
  int jump_back;
  const uint32_t *src = in->data;
  uint32_t *dst;
  unsigned i, j;
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  out = dang_new (DangMatrix, 1);
  out->ref_count = 1;
  out->n_cols = src_rows;
  out->n_rows = src_cols;
  out->data = dst = dang_new (uint32_t, src_rows * src_cols);
  jump_back = src_rows * src_cols - 1;
  for (i = 0; i < src_rows; i++)
    {
      for (j = 0; j < src_cols; j++)
        {
          *dst = *src++;
          dst += src_rows;
        }
      dst -= jump_back;
    }
  * (DangMatrix **) rv_out = out;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_transpose__memcpy8)
{
  DangMatrix *in = * (DangMatrix **) args[0];
  DangMatrix *out;
  unsigned src_rows = in->n_rows;
  unsigned src_cols = in->n_cols;
  int jump_back;
  const uint64_t *src = in->data;
  uint64_t *dst;
  unsigned i, j;
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  out = dang_new (DangMatrix, 1);
  out->ref_count = 1;
  out->n_cols = src_rows;
  out->n_rows = src_cols;
  out->data = dst = dang_new (uint64_t, src_rows * src_cols);
  jump_back = src_rows * src_cols - 1;
  for (i = 0; i < src_rows; i++)
    {
      for (j = 0; j < src_cols; j++)
        {
          *dst = *src++;
          dst += src_rows;
        }
      dst -= jump_back;
    }
  * (DangMatrix **) rv_out = out;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_transpose__memcpy)
{
  DangMatrix *in = * (DangMatrix **) args[0];
  DangMatrix *out;
  unsigned src_rows = in->n_rows;
  unsigned src_cols = in->n_cols;
  int step_forward, jump_back;
  const char *src = in->data;
  char *dst;
  unsigned i, j;
  unsigned elt_size = (unsigned) func_data;
  DANG_UNUSED (error);
  out = dang_new (DangMatrix, 1);
  out->ref_count = 1;
  out->n_cols = src_rows;
  out->n_rows = src_cols;
  out->data = dst = dang_malloc (src_rows * src_cols * elt_size);
  step_forward = src_rows * elt_size;
  jump_back = (src_rows * src_cols - 1) * elt_size;
  for (i = 0; i < src_rows; i++)
    {
      for (j = 0; j < src_cols; j++)
        {
          memcpy (dst, src, elt_size);
          src += elt_size;
          dst += step_forward;
        }
      dst -= jump_back;
    }
  * (DangMatrix **) rv_out = out;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_transpose__virtual)
{
  DangMatrix *in = *(DangMatrix**)args[0];
  DangMatrix *out;
  unsigned src_rows = in->n_rows;
  unsigned src_cols = in->n_cols;
  int step_forward, jump_back;
  const char *src = in->data;
  char *dst;
  unsigned i, j;
  DangValueType *elt_type = func_data;
  unsigned elt_size = elt_type->sizeof_instance;
  DANG_UNUSED (error);
  out = dang_new (DangMatrix, 1);
  out->ref_count = 1;
  out->n_cols = src_rows;
  out->n_rows = src_cols;
  out->data = dst = dang_malloc (src_rows * src_cols * elt_size);
  step_forward = src_rows * elt_size;
  jump_back = (src_rows * src_cols - 1) * elt_size;
  for (i = 0; i < src_rows; i++)
    {
      for (j = 0; j < src_cols; j++)
        {
          elt_type->init_assign (elt_type, dst, src);
          src += elt_size;
          dst += step_forward;
        }
      dst -= jump_back;
    }
  * (DangMatrix **) rv_out = out;
  return TRUE;
}

static DangFunction *
try_sig__matrix_transpose  (DangMatchQuery *query,
                            void *data,
                            DangError **error)
{
  DangSignature *sig;
  DangValueType *elt_type, *type;
  DangValueTypeTensor *ttype;
  DangFunctionParam param;
  DangFunction *rv;
  void *func_data;
  DangSimpleCFunc func;
  DANG_UNUSED (data);
  DANG_UNUSED (error);
  if (query->n_elements != 1
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return NULL;
  type = query->elements[0].info.simple_input;
  if (!dang_value_type_is_tensor (type))
    return NULL;
  ttype = (DangValueTypeTensor*) type;
  if (ttype->rank != 2)
    return NULL;
  elt_type = ttype->element_type;
  if (elt_type->init_assign == NULL)
    {
      func_data = (void *) elt_type->sizeof_instance;
      if (elt_type->sizeof_instance == 4)
        func = do_transpose__memcpy4;
      else if (elt_type->sizeof_instance == 8)
        func = do_transpose__memcpy8;
      else
        func = do_transpose__memcpy;
    }
  else
    {
      func_data = elt_type;
      func = do_transpose__virtual;
    }
  param.dir = DANG_FUNCTION_PARAM_IN;
  param.name = NULL;
  param.type = type;
  sig = dang_signature_new (type, 1, &param);
  rv = dang_function_new_simple_c (sig, func, func_data, NULL);
  dang_signature_unref (sig);
  return rv;
}

static DANG_SIMPLE_C_FUNC_DECLARE (do_diag_vector_to_matrix)
{
  DangVector *a = *(DangVector**) args[0];
  DangMatrix *b;
  const char *src;
  char *dst;
  DangValueType *elt_type = func_data;
  unsigned elt_size = elt_type->sizeof_instance;
  unsigned dst_jump = elt_size * (a->len + 1);
  unsigned i;
  DANG_UNUSED (error);
  b = dang_new (DangMatrix, 1);
  b->ref_count = 1;
  b->n_rows = b->n_cols = a->len;
  b->data = dang_malloc0 (a->len * a->len * elt_size);
  src = a->data;
  dst = b->data;
  if (elt_type->init_assign)
    {
      for (i = 0; i < a->len; i++)
        {
          elt_type->init_assign (elt_type, dst, src);
          dst += dst_jump;
          src += elt_size;
        }
    }
  else
    {
      for (i = 0; i < a->len; i++)
        {
          memcpy (dst, src, elt_size);
          dst += dst_jump;
          src += elt_size;
        }
    }
  * (DangMatrix **) rv_out = b;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_diag_matrix_to_vector)
{
  const DangMatrix *a = *(DangMatrix**) args[0];
  DangVector *b;
  const char *src;
  char *dst;
  DangValueType *elt_type = func_data;
  unsigned elt_size = elt_type->sizeof_instance;
  unsigned src_jump = elt_size * (a->n_cols + 1);
  unsigned i;
  if (a->n_cols != a->n_rows)
    {
      dang_set_error (error, "matrix argument to 'diag' must be square, got %ux%u",
                      a->n_rows, a->n_cols);
      return FALSE;
    }
  b = dang_new (DangVector, 1);
  b->ref_count = 1;
  b->len = a->n_rows;
  b->data = dang_malloc (a->n_rows * elt_size);
  src = a->data;
  dst = b->data;
  if (elt_type->init_assign)
    {
      for (i = 0; i < a->n_rows; i++)
        {
          elt_type->init_assign (elt_type, dst, src);
          src += src_jump;
          dst += elt_size;
        }
    }
  else
    {
      for (i = 0; i < a->n_rows; i++)
        {
          memcpy (dst, src, elt_size);
          src += src_jump;
          dst += elt_size;
        }
    }
  * (DangVector **) rv_out = b;
  return TRUE;
}

static DangFunction *
try_sig__diag  (DangMatchQuery *query,
                void *data,
                DangError **error)
{
  DangSignature *sig;
  DangValueType *elt_type, *type;
  DangValueTypeTensor *ttype;
  DangFunctionParam param;
  DangFunction *rv;
  DangSimpleCFunc func;
  DangValueType *rv_type;
  DANG_UNUSED (data);
  DANG_UNUSED (error);
  if (query->n_elements != 1
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return NULL;
  type = query->elements[0].info.simple_input;
  if (!dang_value_type_is_tensor (type))
    return NULL;
  ttype = (DangValueTypeTensor*) type;
  if (ttype->rank > 2)
    return NULL;
  elt_type = ttype->element_type;
  if (ttype->rank == 1)
    {
      func = do_diag_vector_to_matrix;
      rv_type = dang_value_type_tensor (elt_type, 2);
    }
  else
    {
      func = do_diag_matrix_to_vector;
      rv_type = dang_value_type_tensor (elt_type, 1);
    }

  param.dir = DANG_FUNCTION_PARAM_IN;
  param.name = NULL;
  param.type = type;
  sig = dang_signature_new (rv_type, 1, &param);
  rv = dang_function_new_simple_c (sig, func, elt_type, NULL);
  dang_signature_unref (sig);
  return rv;
}

/* --- operator_concat --- */
typedef struct _ConcatInfo ConcatInfo;
struct _ConcatInfo
{
  DangValueType *element_type;
  unsigned a_rank;    /* rank of the first argument.
                         the function can figure the second rank out.
                         a_rank may be 0 in the case of concat(element, vector).
                       */
};
static void
init_assign_loop (DangValueType *type,
                  void *dst,
                  const void *src,
                  unsigned N)
{
  unsigned size = type->sizeof_instance;
  while (N--)
    {
      type->init_assign (type, dst, src);
      dst = (char*)dst + size;
      src = (char*)src + size;
    }
}

static DANG_SIMPLE_C_FUNC_DECLARE (do_concat_element_vector)
{
  const void *a = args[0];
  DangVector *b = * (DangVector**) args[1];
  DangVector *rv;
  ConcatInfo *ci = func_data;
  DangValueType *type = ci->element_type;
  unsigned elt_size = type->sizeof_instance;
  DANG_UNUSED (error);
  rv = dang_new (DangVector, 1);
  rv->ref_count = 1;
  rv->len = b->len + 1;
  rv->data = dang_malloc (rv->len * elt_size);
  if (type->init_assign == NULL)
    {
      memcpy (rv->data, a, elt_size);
      memcpy ((char*)rv->data + elt_size, b->data, elt_size * b->len);
    }
  else
    {
      type->init_assign (type, rv->data, a);
      init_assign_loop (type, (char*)rv->data + elt_size, b->data, b->len);
    }
  * (DangVector **) rv_out = rv;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_concat_vector_element)
{
  DangVector *a = * (DangVector**) args[0];
  const void *b = args[1];
  DangVector *rv = rv_out;
  ConcatInfo *ci = func_data;
  DangValueType *type = ci->element_type;
  unsigned elt_size = type->sizeof_instance;
  DANG_UNUSED (error);
  rv = dang_new (DangVector, 1);
  rv->ref_count = 1;
  rv->len = a->len + 1;
  rv->data = dang_malloc (rv->len * elt_size);
  if (type->init_assign == NULL)
    {
      memcpy (rv->data, a->data, elt_size * a->len);
      memcpy ((char*)rv->data + elt_size * a->len, b, elt_size);
    }
  else
    {
      init_assign_loop (type, (char*)rv->data + elt_size, a->data, a->len);
      type->init_assign (type, (char*)rv->data + elt_size * a->len, b);
    }
  * (DangVector **) rv_out = rv;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_concat_tensor_tensor)
{
  DangTensor *a = args[0];
  DangTensor *b = args[1];
  DangTensor *rv;
  ConcatInfo *ci = func_data;
  DangValueType *type = ci->element_type;
  unsigned elt_size = type->sizeof_instance;
  unsigned subsize = 1;
  unsigned i;
  for (i = 1; i < ci->a_rank; i++)
    {
      subsize *= b->sizes[i];
      if (a->sizes[i] != b->sizes[i])
        {
          dang_set_error (error, "...");
          return FALSE;
        }
    }
  rv = dang_malloc (DANG_TENSOR_SIZEOF (ci->a_rank));
  rv->ref_count = 1;
  rv->sizes[0] = a->sizes[0] + b->sizes[0];
  for (i = 1; i < ci->a_rank; i++)
    rv->sizes[i] = a->sizes[i];
  rv->data = dang_malloc (elt_size * subsize * rv->sizes[0]);
  if (type->init_assign != NULL)
    {
      init_assign_loop (type, rv->data, a->data, subsize * a->sizes[0]);
      init_assign_loop (type, (char*)rv->data + elt_size * subsize * a->sizes[0],
              b->data, subsize * b->sizes[0]);
    }
  else
    {
      memcpy (rv->data, a->data, elt_size * subsize * a->sizes[0]);
      memcpy ((char*)rv->data + elt_size * subsize * a->sizes[0],
              b->data, elt_size * subsize * b->sizes[0]);
    }
  * (DangTensor **) rv_out = rv;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_concat_matrix_vector)
{
  /* Fake up a tensor of rank a_rank for b (which is of rank a_rank-1) */
  ConcatInfo *ci = func_data;
  DangTensor *b;
  void *new_args[2];
  b = dang_alloca (sizeof (DangTensor) + sizeof(unsigned) * ci->a_rank);
  b->sizes[0] = 1;
  b->data = ((DangTensor*)args[1])->data;
  memcpy (b->sizes + 1, ((DangTensor*)args[1])->sizes,
          (ci->a_rank - 1) * sizeof(unsigned));

  /* Chain to general case */
  new_args[0] = args[0];
  new_args[1] = b;
  return do_concat_tensor_tensor (new_args, rv_out, ci, error);
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_concat_vector_matrix)
{
  /* Fake up a tensor of rank a_rank+1 for a (which is of rank a_rank-1) */
  ConcatInfo *ci = func_data;
  DangTensor *a;
  void *new_args[2];
  ConcatInfo new_ci;
  a = dang_alloca (sizeof (DangTensor) + sizeof(unsigned) * ci->a_rank);
  a->sizes[0] = 1;
  a->data = (*(DangTensor**)args[0])->data;
  memcpy (a->sizes + 1, ((DangTensor*)args[0])->sizes,
          (ci->a_rank) * sizeof(unsigned));

  /* Chain to general case */
  new_args[0] = &a;
  new_args[1] = args[1];
  new_ci.element_type = ci->element_type;
  new_ci.a_rank = ci->a_rank + 1;
  return do_concat_tensor_tensor (new_args, rv_out, &new_ci, error);
}
static DangFunction *
try_sig__operator_concat  (DangMatchQuery *query,
                           void *data,
                           DangError **error)
{
  DangValueType *a, *b;
  unsigned a_rank, out_rank;
  ConcatInfo *concat_info;
  DangFunctionParam params[2];
  DangSignature *sig;
  DangFunction *rv;
  DangSimpleCFunc func;
  DangValueType *elt_type;

  DANG_UNUSED (error);
  DANG_UNUSED (data);

  if (query->n_elements != 2
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || query->elements[1].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return FALSE;
  a = query->elements[0].info.simple_input;
  b = query->elements[1].info.simple_input;
  if (!dang_value_type_is_tensor (a)
   && !dang_value_type_is_tensor (b))
    return FALSE;
  if (a == b)
    {
      /* concat two equal rank tensors */
      func = do_concat_tensor_tensor;
      elt_type = ((DangValueTypeTensor*)a)->element_type;
      a_rank = ((DangValueTypeTensor*)a)->rank;
      out_rank = a_rank;
    }
  else if (dang_value_type_is_tensor (a))
    {
      DangValueTypeTensor *ta = (DangValueTypeTensor*) a;
      a_rank = ta->rank;
      if (ta->rank == 1 && ta->element_type == b)
        {
          /* concat vector with element */
          func = do_concat_vector_element;
          elt_type = b;
          out_rank = 1;
          goto success;
        }
      else if (!dang_value_type_is_tensor (b))
        return FALSE;
      {
        DangValueTypeTensor *tb = (DangValueTypeTensor*) b;
        if (ta->element_type != tb->element_type)
          return FALSE;
        if (ta->rank + 1 == tb->rank)
          {
             /* concat vector with matrix (or higher rank equiv) */
            func = do_concat_vector_matrix;
            elt_type = ta->element_type;
            out_rank = tb->rank;
          }
        else if (ta->rank == tb->rank + 1)
          {
            /* concat matrix with vector (or higher rank equiv) */
            func = do_concat_matrix_vector;
            elt_type = ta->element_type;
            out_rank = ta->rank;
          }
        else
          return FALSE;
      }
    }
  else
    {
      DangValueTypeTensor *tb = (DangValueTypeTensor*) b;
      if (tb->rank != 1)
        return FALSE;
      if (tb->element_type == a)
        {
          /* concat element with vector */
          elt_type = a;
          func = do_concat_element_vector;
          out_rank = 1;
        }
      else
        return FALSE;
    }

success:
  concat_info = dang_new (ConcatInfo, 1);
  concat_info->a_rank = a_rank;
  concat_info->element_type = elt_type;
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].name = NULL;
  params[0].type = a;
  params[1].dir = DANG_FUNCTION_PARAM_IN;
  params[1].name = NULL;
  params[1].type = b;
  sig = dang_signature_new (dang_value_type_tensor (elt_type, out_rank),
                            2, params);
  rv = dang_function_new_simple_c (sig, func, concat_info, dang_free);
  dang_signature_unref (sig);
  return rv;
}

/* Concatenate a vector of vectors into one. */
static DANG_SIMPLE_C_FUNC_DECLARE (concat_array_of_tensors)
{
  DangVector *in = * (DangVector **) args[0];
  DangTensor *out;
  ConcatInfo *ci = func_data;
  unsigned rank = ci->a_rank;
  unsigned major_length_total;
  unsigned inner_size = DANG_TENSOR_SIZEOF (rank);
  DangTensor *last = in->data;
  DangTensor *sub = (DangTensor*)((char*)last + inner_size);
  unsigned elt_size;
  unsigned i, d;
  unsigned n_nonmajor_elts;
  out = dang_malloc (inner_size);
  out->ref_count = 1;
  *(DangTensor **) rv_out = out;
  if (in->len == 0)
    {
      for (i = 0; i < rank; i++)
        out->sizes[i] = 0;
      return TRUE;
    }
  major_length_total = last->sizes[0];
  for (i = 1; i < in->len; i++)
    {
      major_length_total += sub->sizes[0];
      for (d = 1; d < rank; d++)
        if (last->sizes[d] != sub->sizes[d])
          {
            dang_set_error (error, "size mismatch in dimension #%u to argument #%u of concat() (size %u v %u)",
                            d+1, i+1, last->sizes[d], sub->sizes[d]);
            dang_free (out);
            return FALSE;
          }
      last = sub;
      sub = (DangTensor*)((char*)last + inner_size);
    }
  n_nonmajor_elts = 1;
  for (d = 1; d < rank; d++)
    {
      n_nonmajor_elts *= last->sizes[d];
      out->sizes[d] = last->sizes[d];
    }
  out->sizes[0] = major_length_total;
  elt_size = ci->element_type->sizeof_instance;
  out->data = dang_malloc (major_length_total * n_nonmajor_elts * elt_size);
  sub = (DangTensor*)in->data;
  if (ci->element_type->init_assign)
    {
      char *at = out->data;
      for (i = 0; i < in->len; i++)
        {
          unsigned count = sub->sizes[0] * n_nonmajor_elts;
          init_assign_loop (ci->element_type, at, sub->data, count);
          at += count * elt_size;
          sub = (DangTensor*)((char*)sub + inner_size);
        }
    }
  else
    {
      char *at = out->data;
      unsigned plane_size = n_nonmajor_elts * elt_size;
      for (i = 0; i < in->len; i++)
        {
          unsigned s = sub->sizes[0] * plane_size;
          memcpy (at, sub->data, s);
          at += s;
          sub = (DangTensor*)((char*)sub + inner_size);
        }
    }
  return TRUE;
}

#if 0
/* ---- operator_assign_concat (<>=) --- */

static DANG_SIMPLE_C_FUNC_DECLARE (do_vector_append_element)
{
  DangVector *vec = args[0];
  const void *value = args[1];
  DangValueTypeTensor *ttype = func_data;
  DangValueType *elt_type = ttype->element_type;
  DANG_UNUSED (rv_out);
  DANG_UNUSED (error);
  if (vec->len == vec->alloced)
    {
      unsigned new_len = vec->alloced;
      if (!DANG_IS_POWER_OF_TWO (new_len))
        {
          new_len = 2;
          while (new_len <= vec->len)
            new_len += new_len;
        }
      else
        {
          new_len *= 2;
        }
      vec->data = dang_realloc (vec->data, elt_type->sizeof_instance * new_len);
      vec->alloced = new_len;
    }
  if (elt_type->init_assign)
    elt_type->init_assign (elt_type,
                           (char*)vec->data
                           + vec->len * elt_type->sizeof_instance,
                           value);
  else
    memcpy ((char*)vec->data + vec->len * elt_type->sizeof_instance,
            value, elt_type->sizeof_instance);
  ++(vec->len);
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (do_vector_append_vector)
{
  DangVector *vec = args[0];
  DangVector *src = args[1];
  DangValueTypeTensor *ttype = func_data;
  DangValueType *elt_type = ttype->element_type;
  DANG_UNUSED (rv_out);
  DANG_UNUSED (error);
  if (vec->len + src->len > vec->alloced)
    {
      unsigned needed = vec->len + src->len;
      unsigned new_len = vec->alloced;
      if (!DANG_IS_POWER_OF_TWO (new_len))
        new_len = 2;
      else
        new_len *= 2;
      while (new_len < needed)
        new_len += new_len;
      vec->data = dang_realloc (vec->data, elt_type->sizeof_instance * new_len);
      vec->alloced = new_len;
    }
  {
    unsigned size = elt_type->sizeof_instance;
    char *dst_data = (char*)vec->data + size * vec->len;
    char *src_data = (char*)src->data;
    unsigned i;
    if (elt_type->init_assign)
      {
        for (i = 0; i < src->len; i++)
          {
            elt_type->init_assign (elt_type, dst_data, src_data);
            dst_data += size;
            src_data += size;
          }
      }
    else
      {
        memcpy (dst_data, src_data, src->len * size);
      }
  }
  vec->len += src->len;
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE (do_tensor_append_tensor)
{
  DangTensor *a = args[0];
  DangTensor *b = args[1];
  DangValueTypeTensor *ttype = func_data;
  DangValueType *elt_type = ttype->element_type;
  unsigned rank = ttype->rank;
  unsigned minor_count = 1;
  unsigned i;
  void *start;
  DANG_UNUSED (rv_out);
  DANG_UNUSED (error);

  for (i = 1; i < rank; i++)
    {
      if (a->sizes[i] != b->sizes[i])
        {
          dang_set_error (error, "size mismatch in dimension #%u to of <>= (size %u v %u)",
                          i+1, a->sizes[i], b->sizes[i]);
          return FALSE;
        }
      minor_count *= a->sizes[i];
    }
  if (b->sizes[0] == 0)
    return TRUE;

  a->data = dang_realloc (a->data,
                           (a->sizes[0] + b->sizes[0])
                         * minor_count
                         * elt_type->sizeof_instance);
  start = (char*)a->data
        + a->sizes[0] * minor_count * elt_type->sizeof_instance;
  a->sizes[0] += b->sizes[0];
  dang_value_bulk_copy (elt_type, start, b->data, minor_count * b->sizes[0]);
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE (do_tensor_append_subtensor)
{
  DangTensor *a = args[0];
  DangTensor *b = args[1];
  DangValueTypeTensor *ttype = func_data;
  DangValueType *elt_type = ttype->element_type;
  unsigned rank = ttype->rank;
  unsigned minor_count = 1;
  unsigned i;
  void *start;
  DANG_UNUSED (rv_out);
  DANG_UNUSED (error);

  for (i = 1; i < rank; i++)
    {
      if (a->sizes[i] != b->sizes[i-1])
        {
          dang_set_error (error, "size mismatch in dimension #%u and #%u to of <>= (size %u v %u)",
                          i+1, i, a->sizes[i], b->sizes[i-1]);
          return FALSE;
        }
      minor_count *= a->sizes[i];
    }

  a->data = dang_realloc (a->data,
                           (a->sizes[0] + 1)
                         * minor_count
                         * elt_type->sizeof_instance);
  start = (char*)a->data
        + a->sizes[0] * minor_count * elt_type->sizeof_instance;
  a->sizes[0] += 1;
  dang_value_bulk_copy (elt_type, start, b->data, minor_count * 1);
  return TRUE;
}

static DangFunction *
try_sig__operator_assign_concat  (DangMatchQuery *query,
                                  void *data,
                                  DangError **error)
{
  DangValueType *type_b;
  DangValueTypeTensor *ttype_a, *ttype_b;
  DangFunctionParam params[2];
  DangSimpleCFunc simple_c;
  DangSignature *sig;
  DangFunction *rv;
  DANG_UNUSED (data);
  if (query->n_elements != 2
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT
   || query->elements[1].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || !dang_value_type_is_tensor (query->elements[0].info.simple_output))
    {
      return NULL;
    }
  ttype_a = (DangValueTypeTensor*) query->elements[0].info.simple_output;
  type_b = query->elements[1].info.simple_input;
  if (ttype_a->rank == 1
   && ttype_a->element_type == type_b)
    {
      simple_c = do_vector_append_element;
      goto success;
    }

  if (!dang_value_type_is_tensor (type_b))
    return NULL;
  ttype_b = (DangValueTypeTensor*) type_b;
  if (ttype_b->element_type != ttype_a->element_type)
    {
      dang_set_error (error, "type mismatch in elements of tensor <>= : %s v %s",
                      ttype_a->element_type->full_name,
                      ttype_b->element_type->full_name);
      return NULL;
    }
  if (ttype_a == ttype_b)
    {
      if (ttype_a->rank == 1)
        simple_c = do_vector_append_vector;
      else
        simple_c = do_tensor_append_tensor;
    }
  else if (ttype_a->rank == ttype_b->rank + 1)
    {
      /* or higher rank equivalents */
      simple_c = do_tensor_append_subtensor;
    }
  else
    {
      dang_set_error (error, "rank mismatch in tensor <>= : %s v %s",
                      ttype_a->base_type.full_name,
                      ttype_b->base_type.full_name);
      return NULL;
    }

success:
  params[0].dir = DANG_FUNCTION_PARAM_INOUT;
  params[0].type = (DangValueType *) ttype_a;
  params[0].name = NULL;
  params[1].dir = DANG_FUNCTION_PARAM_IN;
  params[1].type = type_b;
  params[1].name = NULL;
  sig = dang_signature_new (NULL, 2, params);
  rv = dang_function_new_simple_c (sig, simple_c, ttype_a, NULL);
  dang_signature_unref (sig);
  return rv;
}
#endif

static DangFunction *
try_sig__concat_array_of_tensors  (DangMatchQuery *query,
                                   void *data,
                                   DangError **error)
{
  DangValueTypeTensor *outer, *inner;
  DangSignature *sig;
  DangFunction *rv;
  ConcatInfo *concat_info = data;
  DangFunctionParam param;
  DANG_UNUSED (error);
  if (query->n_elements != 1
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || !dang_value_type_is_tensor (query->elements[0].info.simple_input))
    return NULL;
  outer = (DangValueTypeTensor*) query->elements[0].info.simple_input;
  if (outer->rank != 1
   || !dang_value_type_is_tensor (outer->element_type))
    return NULL;
  inner = (DangValueTypeTensor*) outer->element_type;

  concat_info = dang_new (ConcatInfo, 1);
  concat_info->a_rank = inner->rank;
  concat_info->element_type = inner->element_type;
  param.dir = DANG_FUNCTION_PARAM_IN;
  param.name = NULL;
  param.type = (DangValueType *) outer;
  sig = dang_signature_new (outer->element_type, 1, &param);
  rv = dang_function_new_simple_c (sig, concat_array_of_tensors,
                                   concat_info, dang_free);
  dang_signature_unref (sig);
  return rv;
}

static DANG_SIMPLE_C_FUNC_DECLARE (do_matrix_rows)
{
  DangVector *out;
  DangVector *rows;
  DangMatrix *in = *(DangMatrix**) args[0];
  const char *src_at = in->data;
  unsigned i, j;
  DangValueType *elt_type = func_data;
  unsigned elt_size = elt_type->sizeof_instance;
  DANG_UNUSED (error);
  out = dang_new (DangVector, 1);
  out->ref_count = 1;
  out->len = in->n_rows;
  out->data = rows = dang_new (DangVector, in->n_rows);
  for (i = 0; i < in->n_rows; i++)
    {
      char *dst_at;
      rows[i].len = in->n_cols;
      rows[i].data = dst_at = dang_malloc (elt_size * in->n_cols);
      if (elt_type->init_assign)
        {
          for (j = 0; j < in->n_cols; j++)
            {
              elt_type->init_assign (elt_type, dst_at, src_at);
              src_at += elt_size;
              dst_at += elt_size;
            }
        }
      else
        {
          for (j = 0; j < in->n_cols; j++)
            {
              memcpy (dst_at, src_at, elt_size);
              src_at += elt_size;
              dst_at += elt_size;
            }
        }
    }
  * (DangVector **) rv_out = out;
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE (do_matrix_cols)
{
  DangVector *out;
  DangVector *cols;
  DangMatrix *in = *(DangMatrix**) args[0];
  const char *src_at = in->data;
  unsigned i, j;
  DangValueType *elt_type = func_data;
  unsigned elt_size = elt_type->sizeof_instance;
  DANG_UNUSED (error);
  unsigned dst_offset = 0;
  out = dang_new (DangVector, 1);
  out->ref_count = 1;
  out->len = in->n_cols;
  out->data = cols = dang_new (DangVector, in->n_cols);
  for (i = 0; i < in->n_cols; i++)
    {
      cols[i].len = in->n_rows;
      cols[i].data = dang_malloc (elt_size * in->n_rows);
    }
  for (i = 0; i < in->n_rows; i++)
    {
      if (elt_type->init_assign)
        {
          for (j = 0; j < in->n_cols; j++)
            {
              elt_type->init_assign (elt_type, (char*)cols[j].data + dst_offset, src_at);
              src_at += elt_size;
            }
        }
      else
        {
          for (j = 0; j < in->n_cols; j++)
            {
              memcpy ((char*)cols[j].data + dst_offset, src_at, elt_size);
              src_at += elt_size;
            }
        }
      dst_offset += elt_size;
    }
  * (DangVector **) rv_out = out;
  return TRUE;
}

static DangFunction *
try_sig__matrix_rows_or_cols  (DangMatchQuery *query,
                               void *data,
                               DangError **error)
{
  DangSignature *sig;
  DangValueType *elt_type, *type;
  DangValueTypeTensor *ttype;
  DangFunctionParam param;
  DangFunction *rv;
  DangValueType *rv_type;
  DANG_UNUSED (data);
  DANG_UNUSED (error);
  if (query->n_elements != 1
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return NULL;
  type = query->elements[0].info.simple_input;
  if (!dang_value_type_is_tensor (type))
    return NULL;
  ttype = (DangValueTypeTensor*) type;
  if (ttype->rank != 2)
    return NULL;
  elt_type = ttype->element_type;
  rv_type = dang_value_type_vector (dang_value_type_vector (elt_type));

  param.dir = DANG_FUNCTION_PARAM_IN;
  param.name = NULL;
  param.type = type;
  sig = dang_signature_new (rv_type, 1, &param);
  rv = dang_function_new_simple_c (sig,
                                   (DangSimpleCFunc) data,
                                   elt_type, NULL);
  dang_signature_unref (sig);
  return rv;
}

/* --- reshape --- */
typedef struct _ReshapeInfo ReshapeInfo;
struct _ReshapeInfo
{
  unsigned input_rank, output_rank;
  DangValueType *element_type;
};
static DANG_SIMPLE_C_FUNC_DECLARE (do_reshape)
{
  ReshapeInfo *ri = func_data;
  DangTensor *in = args[0];
  unsigned input_n_elements = in->sizes[0];
  unsigned output_n_elements;
  unsigned i;
  unsigned elt_size;
  DangTensor *out = rv_out;
  for (i = 1; i < ri->input_rank; i++)
    input_n_elements *= in->sizes[i];
  output_n_elements = * (uint32_t *) args[1];
  for (i = 1; i < ri->output_rank; i++)
    output_n_elements *= * (uint32_t *) args[i+1];

  if (input_n_elements != output_n_elements)
    {
      dang_set_error (error, "size mismatch in reshape: array had %u elements, but new shape requires %u",
                      input_n_elements, output_n_elements);
      return FALSE;
    }

  for (i = 0; i < ri->output_rank; i++)
    out->sizes[i] = * (uint32_t *) args[i+1];
  elt_size = ri->element_type->sizeof_instance;
  out->data = dang_malloc (elt_size * output_n_elements);
  if (ri->element_type->init_assign)
    {
      char *out_at = out->data;
      const char *in_at = in->data;
      for (i = 0; i < output_n_elements; i++)
        {
          ri->element_type->init_assign (ri->element_type, out_at, in_at);
          out_at += elt_size;
          in_at += elt_size;
        }
    }
  else
    memcpy (out->data, in->data, elt_size * output_n_elements);
  return TRUE;
}

static DangFunction *
try_sig__reshape  (DangMatchQuery *query,
                   void *data,
                   DangError **error)
{
  unsigned i;
  DangSignature *sig;
  DangFunction *rv;
  DangFunctionParam *params;
  DangValueTypeTensor *ttype;
  ReshapeInfo *reshape_info;
  DANG_UNUSED (data);
  if (query->n_elements < 2)
    return NULL;
  for (i = 0; i < query->n_elements; i++)
    if (query->elements[i].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
      return NULL;
  if (!dang_value_type_is_tensor (query->elements[0].info.simple_input))
    return NULL;
  for (i = 1; i < query->n_elements; i++)
    if (query->elements[i].info.simple_input != dang_value_type_int32 ()
     && query->elements[i].info.simple_input != dang_value_type_uint32 ())
      {
        dang_set_error (error, "expected int as param #%u to reshape(), got %s",
                        i+2,
                        query->elements[i].info.simple_input->full_name);
        return NULL;
      }
  params = dang_newa (DangFunctionParam, query->n_elements);
  for (i = 0; i < query->n_elements; i++)
    {
      params[i].dir = DANG_FUNCTION_PARAM_IN;
      params[i].name = NULL;
      params[i].type = query->elements[i].info.simple_input;
    }
  ttype = (DangValueTypeTensor*) query->elements[0].info.simple_input;
  sig = dang_signature_new (dang_value_type_tensor (ttype->element_type,
                                                    query->n_elements - 1),
                            query->n_elements, params);
  reshape_info = dang_new (ReshapeInfo, 1);
  reshape_info->input_rank = ttype->rank;
  reshape_info->output_rank = query->n_elements - 1;
  reshape_info->element_type = ttype->element_type;
  rv = dang_function_new_simple_c (sig, do_reshape, reshape_info, dang_free);
  dang_signature_unref (sig);
  return rv;
}
/* multiply */
static dang_boolean
multiply_check_matrix_sizes (DangTensor *a,
                             DangTensor *b,
                             DangError **error)
{
  if (a->sizes[1] != b->sizes[0])
    {
      dang_set_error (error, "size mismatch left matrix has %u columns versus right matrix with %u rows",
                      a->sizes[1], b->sizes[0]);
      return FALSE;
    }
  return TRUE;
}

/* --- _dang_tensor_init() --- */
/* probably a useful generic helper */
static void
add_variadic_c_family_data (DangNamespace *ns,
                            const char    *long_name,
                            const char    *name,
                            DangFunctionTrySigFunc func,
                            void *func_data)
{
  DangFunctionFamily *family;
  DangError *error = NULL;
  family = dang_function_family_new_variadic_c (long_name, func, func_data, NULL);
  if (!dang_namespace_add_function_family (ns, name, family, &error))
    dang_die ("dang_namespace_add_function_family(%s) failed: %s",
              name, error->message);
  dang_function_family_unref (family);
}
#define add_variadic_c_family(ns, long_name, name, func) \
  add_variadic_c_family_data(ns, long_name, name, func, NULL)

#define DEFINE_MATRIX_MULTIPLY(type, ctype)                     \
static DANG_SIMPLE_C_FUNC_DECLARE(multiply_matrices__##type)    \
{                                                               \
  DangTensor *a = args[0];                                      \
  DangTensor *b = args[1];                                      \
  DangTensor *rv = rv_out;                                      \
  unsigned na, nb, nc, i, j, k;                                 \
  ctype *out;                                                   \
  DANG_UNUSED (func_data);                                      \
  if (!multiply_check_matrix_sizes (a, b, error))               \
    return FALSE;                                               \
                                                                \
  na = rv->sizes[0] = a->sizes[0];                              \
  nb = a->sizes[1];                                             \
  nc = rv->sizes[1] = b->sizes[1];                              \
  out = rv->data = dang_new (ctype, rv->sizes[0] * rv->sizes[1]);\
                                                                \
  for (i = 0; i < na; i++)                                      \
    for (j = 0; j < nc; j++)                                    \
      {                                                         \
        ctype elt = 0;                                          \
        const ctype *in_a = (ctype*)a->data + i * nb;           \
        const ctype *in_b = (ctype*)b->data + j;                \
        for (k = 0; k < nb; k++)                                \
          {                                                     \
            elt += *in_a * *in_b;                               \
            in_a++;                                             \
            in_b += nc;                                         \
          }                                                     \
        *out++ = elt;                                           \
      }                                                         \
  return TRUE;                                                  \
}

#define DEFINE_DOT_PRODUCT(type, ctype)                         \
static DANG_SIMPLE_C_FUNC_DECLARE(dot_product__##type)          \
{                                                               \
  DangVector *a = args[0];                                      \
  DangVector *b = args[1];                                      \
  unsigned i, len;                                              \
  ctype rv = 0;                                                 \
  ctype *in_a = a->data;                                        \
  ctype *in_b = b->data;                                        \
  DANG_UNUSED (func_data);                                      \
  if (a->len != b->len)                                         \
    {                                                           \
      dang_set_error (error, "size mismatch in dot-product");   \
      return FALSE;                                             \
    }                                                           \
  len = a->len;                                                 \
  for (i = 0; i < len; i++)                                     \
    {                                                           \
      rv += *in_a * *in_b;                                      \
      in_a++;                                                   \
      in_b++;                                                   \
    }                                                           \
  * (ctype *) rv_out = rv;                                      \
  return TRUE;                                                  \
}

DEFINE_MATRIX_MULTIPLY(float, float);
DEFINE_MATRIX_MULTIPLY(double, double);
DEFINE_MATRIX_MULTIPLY(int32, int32_t);
DEFINE_DOT_PRODUCT(float, float);
DEFINE_DOT_PRODUCT(double, double);
DEFINE_DOT_PRODUCT(int32, int32_t);

void
_dang_tensor_init (DangNamespace *the_ns)
{
  DangError *error = NULL;
  //DangValueType *vector_type;
  //DangValueType *matrix_type;
  DangFunctionFamily *family;
  static VariadicSubstringsData vsd_to_string = {
    to_string__tensor,
    FALSE
  };
  static VariadicSubstringsData vsd_box_form = {
    box_form__tensor,
    TRUE
  };

  family = dang_function_family_new_variadic_c ("tensor_to_string",
                                                variadic_c__generic_substrings, 
                                                &vsd_to_string,
                                                NULL);
  if (!dang_namespace_add_function_family (the_ns, "to_string",
                                           family, &error))
    dang_die ("adding 'to_string' for tensor failed");
  dang_function_family_unref (family);

  family = dang_function_family_new_variadic_c ("operator_equal(vector)",
                                                variadic_c__operator_equal, 
                                                (void*)0,
                                                NULL);
  if (!dang_namespace_add_function_family (the_ns, "operator_equal",
                                           family, &error))
    dang_die ("adding 'operator_equal' for tensor failed");
  dang_function_family_unref (family);

  family = dang_function_family_new_variadic_c ("operator_notequal(vector)",
                                                variadic_c__operator_equal, 
                                                (void*)1,
                                                NULL);
  if (!dang_namespace_add_function_family (the_ns, "operator_notequal",
                                           family, &error))
    dang_die ("adding 'operator_notequal' for tensor failed");
  dang_function_family_unref (family);

  family = dang_function_family_new_variadic_c ("dims",
                                                variadic_c__dims,
                                                NULL, NULL);
  if (!dang_namespace_add_function_family (the_ns, "dims",
                                           family, &error))
    dang_die ("adding 'dims' for tensor failed");
  dang_function_family_unref (family);

  family = dang_function_family_new_variadic_c ("tensor_box_form_to_string",
                                                variadic_c__generic_substrings, 
                                                &vsd_box_form,
                                                NULL);
  if (!dang_namespace_add_function_family (the_ns, "box_form",
                                           family, &error))
    dang_die ("adding 'to_string' for tensor failed");
  dang_function_family_unref (family);

  /* define '*' to be matrix-multiply or dot-product or scalar multiply;
     define '+', '-' to work element-wise
   */
#define REGISTER_STD_OPS(elt_type) \
  do { \
  DangValueType *element_type = dang_value_type_##elt_type (); \
  DangValueType *vector_type = dang_value_type_vector (element_type); \
  DangValueType *matrix_type = dang_value_type_matrix (element_type); \
  DangFunctionParam params[2]; \
  DangSignature *sig; \
  params[0].dir = DANG_FUNCTION_PARAM_IN; \
  params[0].name = NULL; \
  params[0].type = matrix_type; \
  params[1] = params[0]; \
  sig = dang_signature_new (matrix_type, 2, params); \
  dang_namespace_add_simple_c (the_ns, "operator_multiply", sig, \
                               multiply_matrices__##elt_type, NULL); \
  dang_signature_unref (sig); \
  params[0].dir = DANG_FUNCTION_PARAM_IN; \
  params[0].name = NULL; \
  params[0].type = vector_type; \
  params[1] = params[0]; \
  sig = dang_signature_new (element_type, 2, params); \
  dang_namespace_add_simple_c (the_ns, "operator_multiply", sig, \
                               dot_product__##elt_type, NULL); \
  dang_signature_unref (sig); \
  }while(0)
  REGISTER_STD_OPS (int32);
  REGISTER_STD_OPS (float);
  REGISTER_STD_OPS (double);


  add_variadic_c_family (the_ns, "tensor_map", "map", try_sig__tensor__map);
  add_variadic_c_family (the_ns, "new_tensor", "new_tensor", try_sig__tensor__new_tensor);
  add_variadic_c_family (the_ns, "vector_grep", "grep", try_sig__vector__grep);
  add_variadic_c_family (the_ns, "vector_length", "length", try_sig__vector__length);
  add_variadic_c_family (the_ns, "tensor_add", "operator_add", try_sig__tensor__operator_add);

  add_variadic_c_family (the_ns, "tensor_subtract", "operator_subtract", try_sig__tensor__operator_subtract);
  add_variadic_c_family (the_ns, "tensor_scalar_mult", "operator_multiply", try_sig__tensor_scalar_multiply);


#define ADD_STATISTIC(full_name, name, id, zero_len_ok)              \
  {                                                                  \
    static StatTypeInfo types[4];                                    \
    static StatInfo info = { name, DANG_N_ELEMENTS (types), types }; \
    types[0].element_type = dang_value_type_int32 ();                \
    types[0].compute_stat = id##__int32;                             \
    types[0].permits_0_len = zero_len_ok;                            \
    types[1].element_type = dang_value_type_uint32 ();               \
    types[1].compute_stat = id##__uint32;                            \
    types[1].permits_0_len = zero_len_ok;                            \
    types[2].element_type = dang_value_type_float ();                \
    types[2].compute_stat = id##__float;                             \
    types[2].permits_0_len = zero_len_ok;                            \
    types[3].element_type = dang_value_type_double ();               \
    types[3].compute_stat = id##__double;                            \
    types[3].permits_0_len = zero_len_ok;                            \
    add_variadic_c_family_data (the_ns, full_name, name,             \
                                try_sig__tensor__statistic,          \
                                &info);                              \
  }
  ADD_STATISTIC("tensor_min", "min", min, FALSE);
  ADD_STATISTIC("tensor_max", "max", max, FALSE);
  ADD_STATISTIC("tensor_sum", "sum", sum, TRUE);
  ADD_STATISTIC("tensor_product", "product", product, TRUE);
  ADD_STATISTIC("tensor_average", "average", average, FALSE);
#undef ADD_STAT



  add_variadic_c_family (the_ns, "matrix_transpose", "transpose", try_sig__matrix_transpose);
  add_variadic_c_family (the_ns, "diag", "diag", try_sig__diag);
  //add_variadic_c_family (the_ns, "tensor_operator_concat", "operator_concat", try_sig__operator_concat);
  add_variadic_c_family_data (the_ns, "matrix_rows", "rows",
                              try_sig__matrix_rows_or_cols,
                              (void*) do_matrix_rows);
  add_variadic_c_family_data (the_ns, "matrix_cols", "cols",
                              try_sig__matrix_rows_or_cols,
                              (void*) do_matrix_cols);
  add_variadic_c_family (the_ns, "reshape", "reshape", try_sig__reshape);

  /* TODO: add length parameter when defining vectors */



  add_variadic_c_family (the_ns, "tensor_operator_concat", "operator_concat",
                         try_sig__operator_concat);
  add_variadic_c_family (the_ns, "tensor_concat", "concat",
                         try_sig__concat_array_of_tensors);

  //add_variadic_c_family (the_ns, "tensor_operator_assign_concat", "operator_assign_concat",
                         //try_sig__operator_assign_concat);
}

//  if (!dang_namespace_add_metafunction (the_ns, "$tensor_map",
//                                        handle_tensor_map, NULL, NULL,
//                                        &error))
//    dang_die ("adding %s failed", "$tensor_map");
static void
free_tensor_tree_recursive (DangValueTypeTensor *a)
{
  if (a->to_string_function)
    dang_function_unref (a->to_string_function);
  if (a->left)
    free_tensor_tree_recursive (a->left);
  if (a->right)
    free_tensor_tree_recursive (a->right);
  dang_free (a->base_type.full_name);
  dang_free ((char*)a->base_type.cast_func_name);
  dang_free (a);
}

void
_dang_tensor_cleanup (void)
{
  DangValueTypeTensor *old_tree = tensor_type_tree;
  tensor_type_tree = NULL;
  if (old_tree)
    free_tensor_tree_recursive (old_tree);
}
