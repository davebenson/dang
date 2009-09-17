#include <string.h>
#include "dang.h"
#include "magic.h"
#include "config.h"
#include "gskrbtreemacros.h"

static int
signatures_compare (DangSignature *a,
                    DangSignature *b)
{
  DangValueType *rv_a, *rv_b;
  unsigned i;
  if (a->n_params < b->n_params)
    return -1;
  if (a->n_params > b->n_params)
    return +1;
  for (i = 0; i < a->n_params; i++)
    {
      if (a->params[i].dir < b->params[i].dir)
        return -1;
      if (a->params[i].dir > b->params[i].dir)
        return +1;
      if (a->params[i].type < b->params[i].type)
        return -1;
      if (a->params[i].type > b->params[i].type)
        return +1;
    }
  rv_a = a->return_type ? a->return_type : dang_value_type_void ();
  rv_b = b->return_type ? b->return_type : dang_value_type_void ();
  if (rv_a < rv_b)
    return -1;
  if (rv_a > rv_b)
    return +1;
  return 0;
}

static DangValueTypeFunction *func_type_tree;
#define COMPARE_FUNC_TREE_NODES(a,b, rv) \
  rv = signatures_compare (a->sig, b->sig)
#define COMPARE_SIG_TO_FUNC_TREE_NODE(a,b, rv) \
  rv = signatures_compare (a, b->sig)
#define GET_TREE()  \
  func_type_tree, DangValueTypeFunction *, \
  GSK_STD_GET_IS_RED, GSK_STD_SET_IS_RED, \
  parent, left, right, COMPARE_FUNC_TREE_NODES

static void
init_assign__function (DangValueType   *type,
                    void            *dst,
                    const void      *src)
{
  DangFunction *rhs = * (DangFunction **) src;
  DANG_UNUSED (type);
  if (rhs == NULL)
    * (DangFunction **) dst = NULL;
  else
    * (DangFunction **) dst = dang_function_ref (rhs);
}
static void
assign__function      (DangValueType   *type,
                    void       *dst,
                    const void *src)
{
  DangFunction *lhs = * (DangFunction **) dst;
  DangFunction *rhs = * (DangFunction **) src;
  DANG_UNUSED (type);
  if (rhs != NULL)
    rhs = dang_function_ref (rhs);
  if (lhs != NULL)
    dang_function_unref (lhs);
  * (DangFunction **) dst = rhs;
}
static void
destruct__function      (DangValueType   *type,
                      void            *value)
{
  DangFunction *str = * (DangFunction **) value;
  DANG_UNUSED (type);
  if (str != NULL)
    dang_function_unref (str);
}
static char *
to_string__function (DangValueType *type,
                  const void    *value)
{
  DangFunction *function = * (DangFunction **) value;
  DANG_UNUSED (type);
  if (function == NULL)
    return dang_strdup ("(null)");
  else
    return dang_strdup_printf ("%s: %p",
                               dang_function_type_name (function->base.type),
                               function);
}
DangValueType *dang_value_type_function (DangSignature *sig)
{
  DangValueTypeFunction *rv;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_TREE (),
                                sig, COMPARE_SIG_TO_FUNC_TREE_NODE,
                                rv);
  if (rv == NULL)
    {
      DangSignature *s;
      DangValueTypeFunction *conflict;
      DangFunctionParam *new_fp = dang_newa (DangFunctionParam, sig->n_params);
      DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
      unsigned i;
      memcpy (new_fp, sig->params, sig->n_params * sizeof (DangFunctionParam));
      dang_string_buffer_printf (&buf, "function<");
      for (i = 0; i < sig->n_params; i++)
        {
          new_fp[i].name = NULL;
          if (i > 0)
            dang_string_buffer_append_c (&buf, ',');
          dang_string_buffer_append (&buf, dang_function_param_dir_name (sig->params[i].dir));
          dang_string_buffer_append_c (&buf, ' ');
          dang_string_buffer_append (&buf, sig->params[i].type->full_name);
        }
      if (sig->return_type && sig->return_type != dang_value_type_void ())
        {
          dang_string_buffer_append (&buf, " : ");
          dang_string_buffer_append (&buf, sig->return_type->full_name);
        }
      dang_string_buffer_append_c (&buf, '>');
      s = dang_signature_new (sig->return_type, sig->n_params, new_fp);

      rv = dang_new0 (DangValueTypeFunction, 1);
      rv->base_type.magic = DANG_VALUE_TYPE_MAGIC;
      rv->base_type.ref_count = 1;
      rv->base_type.full_name = dang_strdup (buf.str);
      dang_free (buf.str);
      rv->base_type.sizeof_instance = sizeof (void*);
      rv->base_type.alignof_instance = DANG_ALIGNOF_POINTER;
      rv->base_type.init_assign = init_assign__function;
      rv->base_type.assign = assign__function;
      rv->base_type.destruct = destruct__function;
      rv->base_type.to_string = to_string__function;
      rv->base_type.internals.is_templated = sig->is_templated;
      rv->sig = s;
      GSK_RBTREE_INSERT (GET_TREE (), rv, conflict);
      dang_assert (conflict == NULL);
    }
  return (DangValueType *) rv;
}
dang_boolean dang_value_type_is_function (DangValueType *type)
{
  return type->init_assign == init_assign__function;
}


static void
free_func_type_tree_recursive (DangValueTypeFunction *fct)
{
  dang_signature_unref (fct->sig);
  dang_free ((char*)fct->base_type.full_name);
  if (fct->left)
    free_func_type_tree_recursive (fct->left);
  if (fct->right)
    free_func_type_tree_recursive (fct->right);
  dang_free (fct);
}
void _dang_value_function_cleanup ()
{
  if (func_type_tree)
    free_func_type_tree_recursive (func_type_tree);
  func_type_tree = NULL;
}
