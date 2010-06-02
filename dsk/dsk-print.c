#include <string.h>
#include <stdio.h>
#include "../gskrbtreemacros.h"
#include "dsk.h"

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
  void              *append_data;
  DskDestroyNotify   append_destroy;

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
  rv->append_data = data;
  rv->append_destroy = destroy;
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
  if (print->append_destroy)
    print->append_destroy (print->append_data);
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

/* --- implementation of print --- */
static dsk_boolean
handle_template_expression (DskPrint *context,
                            const char *expr,
                            unsigned   *expr_len_out,
                            DskError **error)
{
  /* For now, the only type of template expr is $variable */
  ...
}

static dsk_boolean
print__internal (DskPrint   *context,
                 const char *str,
                 DskError  **error)
{
  const char *at = str;
  const char *last_at = at;
  while (*at)
    {
      if (*at == '$')
        {
          if (at[1] == '$')
            {
              /* call append - include the '$' */
              if (!context->append (at-last_at+1, (uint8_t*) last_at, 
                                    context->append_data, error))
                return DSK_FALSE;

              at += 2;
              last_at = at;
            }
          else
            {
              unsigned expr_len;
              /* call append on any uninterpreted data */
              if (at > last_at
               && !context->append (at-last_at, (uint8_t*) last_at, 
                                    context->append_data, error))
                return DSK_FALSE;

              /* parse template expression */
              if (!handle_template_expression (context, at, &expr_len, error))
                return DSK_FALSE;
              at += expr_len;
            }
        }
      else
        at++;
    }
  if (last_at != at)
    {
      if (!context->append (at-last_at, (uint8_t*) last_at, 
                            context->append_data, error))
        return DSK_FALSE;
    }
  return DSK_TRUE;
}

void
dsk_print (DskPrint *context,
           const char *template_string)
{
  DskError *error = NULL;
  if (context == NULL)
    context = get_default_context ();
  if (!print__internal (context, template_string, &error)
   || !context->append (1, "\n", context->append_data, &error))
    {
      dsk_warning ("error in dsk_print: %s", error->message);
      dsk_error_unref (error);
    }
}

/* --- quoting support --- */
void dsk_print_set_quoted_buffer   (DskPrint    *context,
                                    const char  *variable_name,
			            const DskBuffer *buffer,
                                    DskPrintStringQuoting quoting_method)
{
  if (buffer->size == 0)
    {
      dsk_print_set_string (context, variable_name, "");
      return;
    }
  switch (quoting_method)
    {
    case DSK_PRINT_STRING_C_QUOTED:
      ...
    case DSK_PRINT_STRING_HEX:
      ...
    case DSK_PRINT_STRING_HEX_PAIRS:
      ...
    case DSK_PRINT_STRING_RAW:
      ...
    case DSK_PRINT_STRING_MYSTERIOUSLY:
      ...
    }
}
void dsk_print_set_quoted_string   (DskPrint    *context,
                                    const char  *variable_name,
			            const char  *raw_string,
                                    DskPrintStringQuoting quoting_method)
{
  dsk_print_set_quoted_binary (context, variable_name,
                               strlen (raw_string_length), raw_string,
                               quoting_method);
}

void dsk_print_set_quoted_binary   (DskPrint    *context,
                                    const char  *variable_name,
                                    size_t       raw_string_length,
			            const char  *raw_string,
                                    DskPrintStringQuoting quoting_method)
{
  DskBuffer buf;
  DskBufferFragment frag;
  frag.buf_start = 0;
  frag.buf = (uint8_t*) raw_string;
  frag.buf_length = strlen (raw_string);
  frag.next = NULL;
  buf.first_frag = buf.last_frag = &frag;
  buf.size = frag.buf_length;
  dsk_print_set_quoted_buffer (content, variable_name, &buf, quoting_method);
}
