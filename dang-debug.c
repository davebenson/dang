#include <stdio.h>
#include "config.h"
#include "gskrbtreemacros.h"
#include "gskqsortmacro.h"
#include "dang.h"

#ifdef DANG_DEBUG
dang_boolean dang_debug_parse = FALSE;
dang_boolean dang_debug_disassemble = FALSE;



static void debug_dump_expr (DangExpr *expr, unsigned indent)
{
  unsigned i;
  fprintf (stderr, "%*s", indent, "");
  //fprintf (stderr, "[rc=%u]",expr->any.ref_count);
  switch (expr->type)
    {
    case DANG_EXPR_TYPE_FUNCTION:
      fprintf (stderr, "function: %s\n", expr->function.name);
      for (i = 0; i < expr->function.n_args; i++)
        debug_dump_expr (expr->function.args[i], indent + 2);
      break;

    case DANG_EXPR_TYPE_BAREWORD:
      fprintf (stderr, "bareword: %s\n", expr->bareword.name);
      break;

    case DANG_EXPR_TYPE_VALUE:
      {
        char *str = dang_value_to_string (expr->value.type, expr->value.value);
        fprintf (stderr, "value: type=%s: %s\n",
                 expr->value.type->full_name,
                 str);
        dang_free (str);
      }
      break;
    }
}
void dang_debug_dump_expr (DangExpr *expr)
{
  debug_dump_expr (expr, 2);
}


typedef struct _SimpleCFuncInfo SimpleCFuncInfo;
struct _SimpleCFuncInfo
{
  char *run_func_ptr;
  DangNamespace *ns;
  const char *run_func_name;
  SimpleCFuncInfo *left, *right, *parent;
  dang_boolean is_red;
};
static SimpleCFuncInfo *simple_c_func_info_tree = NULL;
#define GET_IS_RED(fi)  (fi)->is_red
#define SET_IS_RED(fi,v)  (fi)->is_red = v
#define RUN_FUNC_INFO_COMPARE(a,b, rv) \
    GSK_QSORT_SIMPLE_COMPARATOR(a->run_func_ptr, b->run_func_ptr, rv)
#define COMPARE_KEY_TO_RFI(a,b, rv) \
      GSK_QSORT_SIMPLE_COMPARATOR(a, b->run_func_ptr, rv)

#define GET_SIMPLE_C_FUNC_INFO_TREE() \
  simple_c_func_info_tree, SimpleCFuncInfo *, GET_IS_RED, SET_IS_RED, \
  parent, left, right, RUN_FUNC_INFO_COMPARE

void dang_debug_register_simple_c (DangSimpleCFunc func,
                                   DangNamespace *ns,
                                   const char *run_func_name)
{
  char *key = (char*)func;
  SimpleCFuncInfo *rfi;
  SimpleCFuncInfo *old = NULL;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_SIMPLE_C_FUNC_INFO_TREE (),
                                key, COMPARE_KEY_TO_RFI,
                                rfi);
  if (rfi != NULL)
    return;

  rfi = dang_new (SimpleCFuncInfo, 1);
  rfi->run_func_ptr = (char*) func;
  rfi->ns = ns;
  rfi->run_func_name = run_func_name;
  GSK_RBTREE_INSERT (GET_SIMPLE_C_FUNC_INFO_TREE(), rfi, old);
}

dang_boolean
dang_debug_query_simple_c (DangSimpleCFunc func,
                           DangNamespace **ns_out,
                           const char **name_out)
{
  char *key = (char*)func;
  SimpleCFuncInfo *rfi;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_SIMPLE_C_FUNC_INFO_TREE (),
                                key, COMPARE_KEY_TO_RFI,
                                rfi);
  if (rfi == NULL)
    return FALSE;
  *ns_out = rfi->ns;
  *name_out = rfi->run_func_name;
  return TRUE;
}

//static void free_run_func_info_recursive (RunFuncInfo *rfi)
//{
//  if (rfi->left)
//    free_run_func_info_recursive (rfi->left);
//  if (rfi->right)
//    free_run_func_info_recursive (rfi->right);
//  dang_free (rfi);
//}
static void free_simple_c_func_info_recursive (SimpleCFuncInfo *rfi)
{
  if (rfi->left)
    free_simple_c_func_info_recursive (rfi->left);
  if (rfi->right)
    free_simple_c_func_info_recursive (rfi->right);
  dang_free (rfi);
}
void _dang_debug_cleanup ()
{
  //free_run_func_info_recursive (run_func_info_tree);
  free_simple_c_func_info_recursive (simple_c_func_info_tree);
  //run_func_info_tree = NULL;
  simple_c_func_info_tree = NULL;
}

#endif
