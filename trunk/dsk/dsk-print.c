#include <string.h>
#include <stdio.h>
#include "../gskrbtreemacros.h"
#include "dsk.h"
#include "dsk-print.h"

typedef struct _VarDef VarDef;
typedef struct _StackNode StackNode;

struct _VarDef
{
  const char *key;		/* value immediately follows structure */
  VarDef *next_in_stack_node;
  VarDef *next_with_same_key;

  /* Only valid if VarDef is in the tree */
  VarDef *left, *right, *parent;
  dsk_boolean is_red;

  /* valid follows */
};
#define COMPARE_VAR_DEFS(a,b, rv) rv = strcmp(a->key, b->key)
#define GET_VARDEF_TREE(context) \
  (context)->tree, VarDef *, GSK_STD_GET_IS_RED, GSK_STD_SET_IS_RED, \
  parent, left, right, COMPARE_VAR_DEFS

struct _StackNode
{
  StackNode *prev;
  VarDef *vars;
};

struct _DskPrint
{
  /* output */
  DskPrintAppendFunc append;
  void              *data;
  DskDestroyNotify   destroy;

  /* dynamically-scoped variables */
  StackNode *top;
  StackNode bottom;

  /* variables, by name */
  VarDef *tree;
};
DskPrint *dsk_print_new    (DskPrintAppendFunc append,
                            void              *data,
			    DskDestroyNotify   destroy)
{
  DskPrint *rv = dsk_malloc (sizeof (DskPrint));
  rv->append = append;
  rv->data = data;
  rv->destroy = destroy;
  rv->top = &rv->bottom;
  rv->tree = NULL;
  return rv;
}
void      dsk_print_free   (DskPrint *print)
{
  for (;;)
    {
      /* free all elements in top */
      StackNode *stack = print->top;
      print->top = stack->prev;
      while (stack->vars)
        {
          VarDef *v = stack->vars;
          stack->vars = v->next_in_stack_node;
          dsk_free (v);
        }
      if (print->top == NULL)
        break;
      dsk_free (stack);
    }
  if (print->destroy)
    print->destroy (print->data);
  dsk_free (print);
}

static DskPrint *
get_default_context (void)
{
  static DskPrint *default_context = NULL;
  if (default_context == NULL)
    default_context = dsk_print_new_fp (stderr);
  return default_context;
}

static void
add_var_def (DskPrint *context,
             VarDef   *def)
{
  VarDef *existing;
  if (context == NULL)
    context = get_default_context ();
  def->next_in_stack_node = context->top->vars;
  context->top->vars = def;

  GSK_RBTREE_INSERT (GET_VARDEF_TREE (context), def, existing);
  if (existing)
    GSK_RBTREE_REPLACE_NODE (GET_VARDEF_TREE (context), existing, def);
  def->next_with_same_key = existing;
}


void dsk_print_set_string          (DskPrint    *context,
                                    const char  *variable_name,
			            const char  *value)
{
  unsigned key_len = strlen (variable_name);
  unsigned value_len = strlen (value);
  VarDef *vd = dsk_malloc (sizeof (VarDef) + key_len + 1 + value_len + 1);
  vd->key = strcpy (dsk_stpcpy ((char*)(vd+1), value) + 1, variable_name);
  add_var_def (context, vd);
}

void dsk_print_push (DskPrint *context)
{
  StackNode *new_node = dsk_malloc (sizeof (StackNode));
  if (context == NULL)
    context = get_default_context ();
  new_node->prev = context->top;
  context->top = new_node;
  new_node->vars = NULL;
}

void dsk_print_pop (DskPrint *context)
{
  StackNode *old;
  if (context == NULL)
    context = get_default_context ();
  if (context->top == &context->bottom)
    {
      dsk_warning ("dsk_print_pop: stack empty");
      return;
    }
  old = context->top;
  context->top = old->prev;

  while (old->vars)
    {
      VarDef *vd = old->vars;
      old->vars = vd->next_in_stack_node;
      if (vd->next_with_same_key == NULL)
        GSK_RBTREE_REMOVE (GET_VARDEF_TREE (context), vd);
      else
        GSK_RBTREE_REPLACE_NODE (GET_VARDEF_TREE (context), vd, vd->next_with_same_key);
    }
  dsk_free (old);
}
