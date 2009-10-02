#include <string.h>
#include "dang.h"
#include "magic.h"
#include "config.h"
#include "gskrbtreemacros.h"

static DangValueTypeArray *array_type_tree;
#define GET_IS_RED(fi)  (fi)->is_red
#define SET_IS_RED(fi,v)  (fi)->is_red = v
#define COMPARE_ARRAY_TREE_NODES(a,b,rv) \
  if(a->rank < b->rank) rv = -1; \
  else if(a->rank > b->rank) rv = 1; \
  else if(a->element_type < b->element_type) rv = -1; \
  else if(a->element_type > b->element_type) rv = 1; \
  else rv = 0;
#define GET_ARRAY_TREE() \
  array_type_tree, DangValueTypeArray *, GET_IS_RED, SET_IS_RED, \
  parent, left, right, COMPARE_ARRAY_TREE_NODES

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

static void
array_init_assign (DangValueType *type,
                   void          *dst,
                   const void    *src)
{
  DangArray *arr = * (DangArray **) src;
  DANG_UNUSED (type);
  * (DangArray **) dst = arr;
  if (arr)
    arr->ref_count += 1;
}

static void
array_assign      (DangValueType *type,
                   void          *dst,
                   const void    *src)
{
  DangArray *arr = * (DangArray **) src;
  DangArray *orig = * (DangArray **) dst;
  if (arr)
    arr->ref_count += 1;
  if (orig)
    {
      if (--(orig->ref_count) == 0)
        {
          DangValueTypeArray *atype = (DangValueTypeArray *) type;
          dang_tensor_unref (atype->tensor_type, orig->tensor);
          dang_free (orig);
        }
    }
  * (DangArray **) dst = arr;
}
static void
array_destruct (DangValueType *type,
                void          *to_destruct)
{
  DangArray *arr = * (DangArray **) to_destruct;
  if (arr && --(arr->ref_count) == 0)
    {
      DangValueTypeArray *atype = (DangValueTypeArray *) type;
      dang_tensor_unref (atype->tensor_type, arr->tensor);
      dang_free (arr);
    }
}

static char *
array_to_string (DangValueType *type,
                 const void   *data)
{
  DangArray *arr = * (DangArray **) data;
  if (arr == NULL)
    return dang_strdup ("(null)");
  else
    {
      DangValueTypeArray *atype = (DangValueTypeArray *) type;
      return dang_tensor_to_string (atype->tensor_type, arr->tensor);
    }
}

static dang_boolean
index_get_ptr_array (DangValueIndexInfo *index_info,
                      void          *container,
                      const void   **indices,
                      void         **rv_ptr_out,
                      DangError    **error)
{
  DangArray *array = *(DangArray**) container;
  DangTensor *tensor;
  DangValueTypeArray *ttype = (DangValueTypeArray *) index_info->owner;
  unsigned rank = ttype->rank;
  uint32_t ind = * (uint32_t*)(indices[0]);
  unsigned overall_ind, i;
  if (array == NULL || array->tensor == NULL)
    {
      dang_tensor_oob_error (error, 0, 0, ind);
      return FALSE;
    }
  tensor = array->tensor;
  if (ind >= tensor->sizes[0])
    {
      dang_tensor_oob_error (error, 0, tensor->sizes[0], ind);
      return FALSE;
    }
  overall_ind = ind;
  for (i = 1; i < rank; i++)
    {
      uint32_t ind = * (uint32_t*)(indices[i]);
      if (ind >= tensor->sizes[i])
        {
          dang_tensor_oob_error (error, i, tensor->sizes[i], ind);
          return FALSE;
        }
      overall_ind *= tensor->sizes[i];
      overall_ind += ind;
    }
  *rv_ptr_out = (char*)tensor->data + overall_ind * ttype->element_type->sizeof_instance;
  return TRUE;
}
static dang_boolean
index_get__array   (DangValueIndexInfo *ii,
                     void          *container,
                     const void   **indices,
                     void          *rv_out,
                     dang_boolean   may_create,
                     DangError    **error)
{
  DangValueType *elt_type = ((DangValueTypeArray*)ii->owner)->element_type;
  void *ptr;
  DANG_UNUSED (may_create);
  if (!index_get_ptr_array (ii, container, indices, &ptr, error))
    return FALSE;
  if (elt_type->init_assign)
    elt_type->init_assign (elt_type, rv_out, ptr);
  else
    memcpy (rv_out, ptr, elt_type->sizeof_instance);
  return TRUE;
}

static void
maybe_copy_on_write (DangValueType *type,
                     DangArray     *array)
{
  if (array->tensor != NULL && array->tensor->ref_count > 1)
    {
      /* copy (since we're writing) */
      DangValueTypeArray *atype = (DangValueTypeArray *) type;
      DangTensor *copy = dang_malloc (DANG_TENSOR_SIZEOF (atype->rank));
      size_t size = atype->element_type->sizeof_instance;
      size_t n_elts = 1;
      unsigned i;
      copy->ref_count = 1;
      for (i = 0; i < atype->rank; i++)
        {
          copy->sizes[i] = array->tensor->sizes[i];
          n_elts *= copy->sizes[i];
        }
      size *= n_elts;
      copy->data = dang_malloc (size);
      dang_value_bulk_copy (atype->element_type, copy->data, array->tensor->data, n_elts);

      dang_tensor_unref (atype->tensor_type, array->tensor);
      array->tensor = copy;
    }
}

static dang_boolean
index_set__array   (DangValueIndexInfo *ii,
                     void          *container,
                     const void   **indices,
                     const void    *element_value,
                     dang_boolean   may_create,
                     DangError    **error)
{
  DangValueType *elt_type = ((DangValueTypeArray*)ii->owner)->element_type;
  void *ptr;
  DangArray *array = *(DangArray **) container;
  DANG_UNUSED (may_create);

  /* implement copy-on-write */
  if (array)
    maybe_copy_on_write (ii->owner, array);

  if (!index_get_ptr_array (ii, container, indices, &ptr, error))
    return FALSE;
  if (elt_type->assign)
    elt_type->assign (elt_type, ptr, element_value);
  else
    memcpy (ptr, element_value, elt_type->sizeof_instance);
  return TRUE;
}

/* ---- operator_assign_concat (<>=) --- */

static DANG_SIMPLE_C_FUNC_DECLARE (do_vector_append_element)
{
  DangArray *array = * (DangArray **) args[0];
  const void *value = args[1];
  DangValueTypeArray *atype = func_data;
  DangValueType *elt_type = atype->element_type;
  DangVector *vec;
  DANG_UNUSED (rv_out);
  if (array == NULL)
    {
      dang_set_error (error, "null pointer exception (array while appending)");
      return FALSE;
    }
  vec = (DangVector *) (array->tensor);
  if (vec == NULL)
    {
      vec = dang_new (DangVector, 1);
      vec->ref_count = 1;
      vec->len = 1;
      array->alloced = 1;
      vec->data = dang_malloc (elt_type->sizeof_instance);
      dang_value_bulk_copy (elt_type, vec->data, value, 1);
      array->tensor = (DangTensor *) vec;
      return TRUE;
    }
  if (array->alloced == array->tensor->sizes[0]
   || array->tensor->ref_count > 1)
    {
      unsigned new_len = array->alloced;
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
      if (vec->ref_count > 1)
        {
          /* must allocate a new tensor */
          DangVector *new_vec = dang_new (DangVector, 1);
          new_vec->ref_count = 1;
          new_vec->len = vec->len;
          new_vec->data = dang_malloc (elt_type->sizeof_instance * new_len);
          dang_value_bulk_copy (atype->element_type, new_vec->data, vec->data, vec->len);
          vec->ref_count--;
          vec = new_vec;
          array->tensor = (DangTensor *) new_vec;
        }
      else /* vec->alloced == vec->length -- must resize */
        {
          /* call realloc() */
          vec->data = dang_realloc (vec->data, elt_type->sizeof_instance * new_len);
        }
      array->alloced = new_len;
    }

  /* copy the final element */
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
  DangArray *array = * (DangArray **) args[0];
  DangVector *src = * (DangVector **) args[1];
  DangVector *dst;
  DangValueTypeArray *atype = func_data;
  DangValueType *elt_type = atype->element_type;
  DANG_UNUSED (rv_out);
  if (array == NULL)
    {
      dang_set_error (error, "null pointer exception (array while appending)");
      return FALSE;
    }
  if (src == NULL)
    return TRUE;
  if (src->len + dst->len > array->alloced
   || dst->ref_count > 1)
    {
      unsigned new_len = array->alloced;
      unsigned min_len = src->len + dst->len;
      if (!DANG_IS_POWER_OF_TWO (new_len))
        new_len = 2;
      while (new_len < min_len)
        new_len += new_len;
      if (dst->ref_count > 1)
        {
          /* must allocate a new tensor */
          DangVector *new_dst = dang_new (DangVector, 1);
          new_dst->ref_count = 1;
          new_dst->len = dst->len;
          new_dst->data = dang_malloc (elt_type->sizeof_instance * new_len);
          dang_value_bulk_copy (atype->element_type, new_dst->data, dst->data, dst->len);
          dst->ref_count--;
          dst = new_dst;
          array->tensor = (DangTensor *) new_dst;
        }
      else /* dst->alloced == dst->length -- must resize */
        {
          /* call realloc() */
          dst->data = dang_realloc (dst->data, elt_type->sizeof_instance * new_len);
        }
      array->alloced = new_len;
    }

  dang_value_bulk_copy (atype->element_type,
                        (char*)dst->data + elt_type->sizeof_instance * dst->len,
                        src->data,
                        src->len);
  dst->len += src->len;
  return TRUE;
}


static DANG_SIMPLE_C_FUNC_DECLARE (do_tensor_append_tensor)
{
  DangArray *array = * (DangArray **) args[0];
  DangTensor *a;
  DangTensor *b = * (DangTensor **) args[1];
  DangValueTypeArray *atype = func_data;
  DangValueType *elt_type = atype->element_type;
  unsigned rank = atype->rank;
  unsigned minor_count = 1;
  unsigned i;
  DANG_UNUSED (rv_out);
  DANG_UNUSED (error);

  if (array == NULL)
    {
      dang_set_error (error, "null pointer exception (tensor while appending)");
      return FALSE;
    }

  /* XXX: technically should check that a is full of 0 length non-first sizes */
  if (b == NULL)
    return TRUE;

  a = array->tensor;
  if (a == NULL)
    {
      array->tensor = b;
      b->ref_count++;
      return TRUE;
    }
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

  if (a->sizes[0] + b->sizes[0] > array->alloced
   || a->ref_count > 1)
    {
      unsigned new_len = array->alloced;
      unsigned min_len = a->sizes[0] + b->sizes[0];
      if (!DANG_IS_POWER_OF_TWO (new_len))
        new_len = 2;
      while (new_len < min_len)
        new_len += new_len;
      if (a->ref_count > 1)
        {
          /* must allocate a new tensor */
          DangTensor *new_a = dang_malloc (DANG_TENSOR_SIZEOF (atype->rank));
          new_a->ref_count = 1;
          memcpy (new_a->sizes, a->sizes, sizeof (unsigned) * atype->rank);
          new_a->data = dang_malloc (elt_type->sizeof_instance * minor_count * new_len);
          dang_value_bulk_copy (elt_type, new_a->data, a->data, minor_count * a->sizes[0]);
          a->ref_count--;
          a = new_a;
          array->tensor = new_a;
        }
      else /* must resize */
        {
          /* call realloc() */
          a->data = dang_realloc (a->data, elt_type->sizeof_instance * minor_count * new_len);
        }
      array->alloced = new_len;
    }

  dang_value_bulk_copy (elt_type,
                        ((char*)a->data) + (elt_type->sizeof_instance * minor_count * a->sizes[0]),
                        b->data,
                        minor_count * b->sizes[0]);
  a->sizes[0] += b->sizes[0];
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE (do_tensor_append_subtensor)
{
  DangArray *array = * (DangArray **) args[0];
  DangTensor *a;
  DangTensor *b = * (DangTensor **) args[1];
  DangValueTypeArray *atype = func_data;
  DangValueType *elt_type = atype->element_type;
  unsigned rank = atype->rank;
  unsigned minor_count = 1;
  unsigned i;
  void *start;
  DANG_UNUSED (rv_out);
  DANG_UNUSED (error);
  if (array == NULL)
    {
      dang_set_error (error, "null pointer exception (tensor while appending)");
      return FALSE;
    }
  a = array->tensor;
  if (b == NULL)
    return TRUE;
  if (a == NULL)
    {
      array->tensor = b;
      b->ref_count++;
      return TRUE;
    }

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

  if (array->alloced == array->tensor->sizes[0]
   || array->tensor->ref_count > 1)
    {
      unsigned new_len = array->alloced;
      unsigned min_len = a->sizes[0] + 1;
      if (!DANG_IS_POWER_OF_TWO (new_len))
        new_len = 2;
      while (new_len < min_len)
        new_len += new_len;
      if (a->ref_count > 1)
        {
          /* must allocate a new tensor */
          DangTensor *new_a = dang_malloc (DANG_TENSOR_SIZEOF (rank));
          new_a->ref_count = 1;
          memcpy (new_a->sizes, a->sizes, sizeof (unsigned) * rank);
          new_a->data = dang_malloc (elt_type->sizeof_instance * minor_count * new_len);
          dang_value_bulk_copy (elt_type, new_a->data, a->data, minor_count * a->sizes[0]);
          a->ref_count--;
          a = new_a;
          array->tensor = new_a;
        }
      else /* must resize */
        {
          /* call realloc() */
          a->data = dang_realloc (a->data, elt_type->sizeof_instance * minor_count * new_len);
        }
      array->alloced = new_len;
    }

  start = (char*)a->data
        + a->sizes[0] * minor_count * elt_type->sizeof_instance;
  dang_value_bulk_copy (elt_type, start, b->data, minor_count * 1);
  a->sizes[0] += 1;
  return TRUE;
}

static DangFunction *
try_sig__operator_assign_concat  (DangMatchQuery *query,
                                  void *data,
                                  DangError **error)
{
  DangValueType *type_b;
  DangValueTypeArray *atype_a;
  DangValueTypeTensor *ttype_b;
  DangFunctionParam params[2];
  DangSimpleCFunc simple_c;
  DangSignature *sig;
  DangFunction *rv;
  DANG_UNUSED (data);
  if (query->n_elements != 2
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT
   || query->elements[1].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || !dang_value_type_is_array (query->elements[0].info.simple_output))
    {
      return NULL;
    }
  atype_a = (DangValueTypeArray*) query->elements[0].info.simple_output;
  type_b = query->elements[1].info.simple_input;
  if (atype_a->rank == 1
   && atype_a->element_type == type_b)
    {
      simple_c = do_vector_append_element;
      goto success;
    }

  if (!dang_value_type_is_tensor (type_b))
    return NULL;
  ttype_b = (DangValueTypeTensor*) type_b;
  if (ttype_b->element_type != atype_a->element_type)
    {
      dang_set_error (error, "type mismatch in elements of tensor <>= : %s v %s",
                      atype_a->element_type->full_name,
                      ttype_b->element_type->full_name);
      return NULL;
    }
  if (atype_a->tensor_type == type_b)
    {
      if (atype_a->rank == 1)
        simple_c = do_vector_append_vector;
      else
        simple_c = do_tensor_append_tensor;
    }
  else if (atype_a->rank == ttype_b->rank + 1)
    {
      /* or higher rank equivalents */
      simple_c = do_tensor_append_subtensor;
    }
  else
    {
      dang_set_error (error, "rank mismatch in tensor <>= : %s v %s",
                      atype_a->base_type.full_name,
                      ttype_b->base_type.full_name);
      return NULL;
    }

success:
  params[0].dir = DANG_FUNCTION_PARAM_INOUT;
  params[0].type = (DangValueType *) atype_a;
  params[0].name = NULL;
  params[1].dir = DANG_FUNCTION_PARAM_IN;
  params[1].type = type_b;
  params[1].name = NULL;
  sig = dang_signature_new (NULL, 2, params);
  rv = dang_function_new_simple_c (sig, simple_c, atype_a, NULL);
  dang_signature_unref (sig);
  return rv;
}
static DANG_SIMPLE_C_FUNC_DECLARE (cast_from_array_to_tensor)
{
  DangArray *array = * (DangArray **) args[0];
  DANG_UNUSED (error);
  DANG_UNUSED (func_data);
  if (array == NULL || array->tensor == NULL)
    * (DangTensor **) rv_out = NULL;
  else
    {
      DangTensor *t = array->tensor;
      ++(t->ref_count);
      * (DangTensor **) rv_out = t;
    }
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE (cast_from_tensor_to_array)
{
  DangTensor *t = * (DangTensor **) args[0];
  DangArray *array;
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  if (t != NULL)
    t->ref_count++;
  array = dang_new (DangArray, 1);
  array->ref_count = 1;
  array->tensor = t;
  array->alloced = t ? t->sizes[0] : 0;
  * (DangArray **) rv_out = array;
  return TRUE;
}

DangValueType *
dang_value_type_array  (DangValueType *element_type,
                        unsigned       rank)
{
  DangValueTypeArray dummy, *out, *conflict = NULL;
  static DangValueType *common_int32_array[MAX_STANDARD_RANK];
  static DangValueType *common_uint32_array[MAX_STANDARD_RANK];
  DangError *error = NULL;
  dummy.element_type = element_type;
  dummy.rank = rank;
  GSK_RBTREE_LOOKUP (GET_ARRAY_TREE (), &dummy, out);
  if (out != NULL)
    return (DangValueType *) out;

  if (array_type_tree == NULL)
    {
      make_repeated_type (common_int32_array, MAX_STANDARD_RANK, dang_value_type_int32());
      make_repeated_type (common_uint32_array, MAX_STANDARD_RANK, dang_value_type_uint32());
    }
  dang_assert (rank > 0);
  out = dang_new0 (DangValueTypeArray, 1);
  out->base_type.magic = DANG_VALUE_TYPE_MAGIC;
  out->base_type.ref_count = 0;
  out->base_type.full_name = dang_strdup_printf ("array<%s,%u>", element_type->full_name, rank);

  out->base_type.sizeof_instance = DANG_SIZEOF_POINTER;
  out->base_type.alignof_instance = DANG_ALIGNOF_POINTER;
  
  out->base_type.init_assign = array_init_assign;
  out->base_type.assign = array_assign;
  out->base_type.destruct = array_destruct;
  out->base_type.to_string = array_to_string;
  out->base_type.cast_func_name = dang_strdup_printf ("operator_cast__array_%u__%s",
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
  out->index_infos[0].get = index_get__array;
  out->index_infos[0].set = index_set__array;
  out->index_infos[0].element_type = element_type;
  out->index_infos[1].next = NULL;
  out->index_infos[1].owner = &out->base_type;
  out->index_infos[1].n_indices = rank;
  out->index_infos[1].element_type = element_type;
  out->index_infos[1].get = index_get__array;
  out->index_infos[1].set = index_set__array;
  out->index_infos[1].element_type = element_type;
  out->tensor_type = dang_value_type_tensor (element_type, rank);

  DangFunctionParam fp;
  DangSignature *sig;
  DangFunction *f;

  /* add cast from array -> tensor */
  fp.dir = DANG_FUNCTION_PARAM_IN;
  fp.name = "in";
  fp.type = &out->base_type;
  sig = dang_signature_new (out->tensor_type, 1, &fp);
  f = dang_function_new_simple_c (sig, cast_from_array_to_tensor, out, NULL);
  if (!dang_namespace_add_function (dang_namespace_default (),
                                    out->tensor_type->cast_func_name,
                                    f, &error))
    dang_die ("dang_namespace_add_function failed: %s", error->message);
  dang_function_unref (f);
  dang_signature_unref (sig);

  /* add cast from tensor -> array */
  fp.dir = DANG_FUNCTION_PARAM_IN;
  fp.name = "in";
  fp.type = out->tensor_type;
  sig = dang_signature_new (&out->base_type, 1, &fp);
  f = dang_function_new_simple_c (sig, cast_from_tensor_to_array, out, NULL);
  if (!dang_namespace_add_function (dang_namespace_default (),
                                    out->base_type.cast_func_name,
                                    f, &error))
    dang_die ("dang_namespace_add_function failed: %s", error->message);
  dang_function_unref (f);
  dang_signature_unref (sig);

  GSK_RBTREE_INSERT (GET_ARRAY_TREE (), out, conflict);
  dang_assert (conflict == NULL);
  return (DangValueType *) out;
}

static DANG_SIMPLE_C_FUNC_DECLARE (simple_c__array_to_string)
{
  DangArray *array = * (DangArray **) args[0];
  DangValueTypeArray *atype = func_data;
  if (array == NULL)
    {
      * (DangString **) rv_out = dang_string_new ("(null)");
    }
  else
    {
      void *datum = &array->tensor;
      void *sub_data[1] = {datum};
      DangValueTypeTensor *ttype = (DangValueTypeTensor*)atype->tensor_type;
      DangFunction *tfunc = ttype->to_string_function;
      if (!dang_function_call_nonyielding_v (tfunc, rv_out, sub_data, error))
        return FALSE;
    }
  return TRUE;
}

static DANG_FUNCTION_TRY_SIG_FUNC_DECLARE (variadic_c__array_to_string)
{
  DangMatchQueryElement subquery_elt;
  DangMatchQuery subquery;
  DangValueTypeArray *atype;
  DangFunctionParam param;
  DangFunction *subfunc;
  DangSignature *sig;
  char *to_string_str = "to_string";
  DangFunction *f;
  DANG_UNUSED (data);
  if (query->n_elements != 1
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || !dang_value_type_is_array (query->elements[0].info.simple_input))
    {
      return NULL;
    }
  atype = (DangValueTypeArray *) query->elements[0].info.simple_input;
  memset (&subquery_elt, 0, sizeof (subquery_elt));
  subquery_elt.type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
  subquery_elt.info.simple_input = atype->element_type;
  subquery.n_elements = 1;
  subquery.elements = &subquery_elt;
  subquery.imports = query->imports;
  subfunc = dang_imports_lookup_function (query->imports,
                                 1, &to_string_str, &subquery,
                                 error);
  if (subfunc == NULL)
    return NULL;

  param.dir = DANG_FUNCTION_PARAM_IN;
  param.type = (DangValueType*) atype;
  param.name = NULL;
  sig = dang_signature_new (dang_value_type_string (), 1, &param);
  f = dang_function_new_simple_c (sig, simple_c__array_to_string, atype, NULL);
  dang_signature_unref (sig);
  return f;
}

void
_dang_array_init (DangNamespace *the_ns)
{
  DangError *error = NULL;
  DangFunctionFamily *family;

  family = dang_function_family_new_variadic_c ("array_to_string",
                                                variadic_c__array_to_string, 
                                                NULL,
                                                NULL);
  if (!dang_namespace_add_function_family (the_ns, "to_string",
                                           family, &error))
    dang_die ("adding 'to_string' for array failed");
  dang_function_family_unref (family);

  family = dang_function_family_new_variadic_c ("array_operator_assign_concat",
                                                try_sig__operator_assign_concat,
                                                NULL,
                                                NULL);
  if (!dang_namespace_add_function_family (the_ns, "operator_assign_concat",
                                           family, &error))
    dang_die ("adding 'to_string' for array failed");
  dang_function_family_unref (family);

}

static void
free_array_tree_recursive (DangValueTypeArray *a)
{
  //if (a->to_string_function)
    //dang_function_unref (a->to_string_function);
  if (a->left)
    free_array_tree_recursive (a->left);
  if (a->right)
    free_array_tree_recursive (a->right);
  dang_free (a->base_type.full_name);
  dang_free ((char*)a->base_type.cast_func_name);
  dang_free (a);
}

void
_dang_array_cleanup (void)
{
  DangValueTypeArray *old_tree = array_type_tree;
  array_type_tree = NULL;
  if (old_tree)
    free_array_tree_recursive (old_tree);
}

dang_boolean
dang_value_type_is_array (DangValueType *type)
{
  return type->assign == array_assign;
}
