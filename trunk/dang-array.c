#include "dang.h"

static DangValueTypeTensor *array_type_tree;
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

DangValueType *
dang_value_type_array  (DangValueType *element_type,
                        unsigned       rank)
{
  DangValueTypeArray dummy, *out, *conflict = NULL;
  ...
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
