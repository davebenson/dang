#include <string.h>
#include "dang.h"
#include "gskrbtreemacros.h"

struct _DangAnnotations
{
  DangExprAnnotation *tree;
};

DangAnnotations *
dang_annotations_new (void)
{
  return dang_new0 (DangAnnotations, 1);
}

static int
expr_annotations_compare (DangExprAnnotation *a,
                          DangExprAnnotation *b)
{
  if (a->expr < b->expr)
    return -1;
  if (a->expr > b->expr)
    return 1;
  if (a->type < b->type)
    return -1;
  if (a->type > b->type)
    return 1;
  return 0;
}


#define COMPARE_EXPR_ANNOTATIONS(a,b, rv) \
  rv = expr_annotations_compare (a, b)
#define GET_TREE(annotations)  \
  (annotations)->tree, DangExprAnnotation *, \
  GSK_STD_GET_IS_RED, GSK_STD_SET_IS_RED, \
  parent, left, right, COMPARE_EXPR_ANNOTATIONS


const char *
dang_expr_tag_type_name (DangExprTagType type)
{
  switch (type)
    {
    case DANG_EXPR_TAG_VALUE: return "value";
    case DANG_EXPR_TAG_NAMESPACE: return "namespace";
    case DANG_EXPR_TAG_FUNCTION_FAMILY: return "function-family";
    case DANG_EXPR_TAG_TYPE: return "type";
    case DANG_EXPR_TAG_STATEMENT: return "statement";
    case DANG_EXPR_TAG_UNTYPED_FUNCTION: return "untyped-function";
    case DANG_EXPR_TAG_CLOSURE: return "closure";
    case DANG_EXPR_TAG_METHOD: return "method";
    case DANG_EXPR_TAG_OBJECT_DEFINE: return "object-define";
    }
  return "*bad tag type*";
}

void
dang_expr_annotation_init(DangAnnotations *annotations,
                          DangExpr        *expr,
                          DangExprAnnotationType type,
                          void            *annotation_to_init)
{
  DangExprAnnotation *a = annotation_to_init;
  DangExprAnnotation *conflict = NULL;
  a->expr = expr;
  a->type = type;
  a->owner = annotations;
  GSK_RBTREE_INSERT (GET_TREE (annotations), a, conflict);
  dang_assert (conflict == NULL);
}

static void
free_expr_annotations_recursive (DangExprAnnotation *e)
{
  switch (e->type)
    {
    case DANG_EXPR_ANNOTATION_TAG:
      {
        DangExprTag *tag = (DangExprTag *) e;
        if (tag->tag_type == DANG_EXPR_TAG_FUNCTION_FAMILY
            && tag->info.ff.function != NULL)
          dang_function_unref (tag->info.ff.function);
        else if (tag->tag_type == DANG_EXPR_TAG_UNTYPED_FUNCTION)
          dang_untyped_function_free (tag->info.untyped_function);
        else if (tag->tag_type == DANG_EXPR_TAG_CLOSURE)
          {
            dang_free (tag->info.closure.closure_var_ids);
            dang_signature_unref (tag->info.closure.sig);
            dang_function_unref (tag->info.closure.stub);
          }
        break;
      }
    default:
      break;
    }
  if (e->left)
    free_expr_annotations_recursive (e->left);
  if (e->right)
    free_expr_annotations_recursive (e->right);
  dang_free (e);
}

void
dang_annotations_free (DangAnnotations *annotations)
{
  if (annotations->tree)
    free_expr_annotations_recursive (annotations->tree);
  dang_free (annotations);
}

void * dang_expr_get_annotation (DangAnnotations *annotations,
                                 DangExpr *expr,
                                 DangExprAnnotationType type)
{
  DangExprAnnotation dummy;
  DangExprAnnotation *out;
  dummy.type = type;
  dummy.expr = expr;
  GSK_RBTREE_LOOKUP (GET_TREE (annotations), &dummy, out);
  return out;
}

dang_boolean dang_expr_annotate_types (DangAnnotations *annotations,
                                       DangExpr    *expr,
                                       DangImports *imports,
				       DangVarTable *var_table,
				       DangError  **error)
{
  DangMetafunction *mf = dang_metafunction_lookup_by_expr (expr);
  if (mf == NULL)
    {
      if (expr->type == DANG_EXPR_TYPE_FUNCTION)
        dang_die ("no handler for %s()", expr->function.name);
      else
        dang_die ("no handler for %s", dang_expr_type_name (expr->type));
    }
  if (mf->annotate == NULL)
    dang_die ("no 'annotate' for function %s", mf->name);
  return mf->annotate (annotations, expr, imports, var_table, error);
}
