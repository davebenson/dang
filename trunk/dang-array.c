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
  DANG_UNUSED (may_create);
  if (!index_get_ptr_array (ii, container, indices, &ptr, error))
    return FALSE;
  if (elt_type->assign)
    elt_type->assign (elt_type, ptr, element_value);
  else
    memcpy (ptr, element_value, elt_type->sizeof_instance);
  return TRUE;
}


DangValueType *
dang_value_type_array  (DangValueType *element_type,
                        unsigned       rank)
{
  DangValueTypeArray dummy, *out, *conflict = NULL;
  static DangValueType *common_int32_array[MAX_STANDARD_RANK];
  static DangValueType *common_uint32_array[MAX_STANDARD_RANK];
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
    dang_error ("dang_namespace_add_function failed: %s", error->message);
  dang_function_unref (f);
  dang_signature_unref (sig);

  /* add cast from tensor -> array */
  fp.dir = DANG_FUNCTION_PARAM_IN;
  fp.name = "in";
  fp.type = out->tensor_type;
  sig = dang_signature_new (out->tensor_type, 1, &fp);
  f = dang_function_new_simple_c (sig, cast_from_tensor_to_array, out, NULL);
  if (!dang_namespace_add_function (dang_namespace_default (),
                                    out->base_type.cast_func_name,
                                    f, &error))
    dang_error ("dang_namespace_add_function failed: %s", error->message);
  dang_function_unref (f);
  dang_signature_unref (sig);

  GSK_RBTREE_INSERT (GET_ARRAY_TREE (), out, conflict);
  dang_assert (conflict == NULL);
  return (DangValueType *) out;
}

void
_dang_array_init (DangNamespace *the_ns)
{
  DangError *error = NULL;
  DangFunctionFamily *family;

  family = dang_function_family_new_variadic_c ("array_to_string",
                                                variadic_c__generic_substrings, 
                                                &vsd_to_string,
                                                NULL);
  if (!dang_namespace_add_function_family (the_ns, "to_string",
                                           family, &error))
    dang_die ("adding 'to_string' for array failed");
  dang_function_family_unref (family);


}

static void
free_array_tree_recursive (DangValueTypeArray *a)
{
  if (a->to_string_function)
    dang_function_unref (a->to_string_function);
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
