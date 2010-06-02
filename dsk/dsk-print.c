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

  unsigned value_length;
  /* value follows */
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
  vd->value_length = value_len;
  add_var_def (context, vd);
}
void dsk_print_set_string_length   (DskPrint    *context,
                                    const char  *variable_name,
                                    unsigned     value_len,
			            const char  *value)
{
  unsigned key_len = strlen (variable_name);
  VarDef *vd = dsk_malloc (sizeof (VarDef) + key_len + 1 + value_len + 1);
  memcpy (vd + 1, value, value_len);
  ((char*)(vd+1))[value_len] = 0;
  vd->key = strcpy ((char*)(vd+1) + value_len + 1, variable_name);
  vd->value_length = value_len;
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
  /* For now, the only type of template expr is $variable or ${variable} */
  unsigned len;
  const char *start;
  VarDef *result;
  dsk_assert (*expr == '$');
  if (expr[1] == '{')
    {
      const char *end;
      start = expr + 2;
      while (dsk_ascii_isspace (*start))
        start++;
      if (!dsk_ascii_isalnum (*start))
        {
          dsk_set_error (error, "unexpected character %s after '${' in dsk_print",
                         dsk_ascii_byte_name (*start));
          return DSK_FALSE;
        }
      len = 1;
      while (dsk_ascii_isalnum (start[len]))
        len++;
      end = start + len;
      while (dsk_ascii_isspace (*end))
        end++;
      if (*end != '}')
        {
          dsk_set_error (error,
                         "unexpected character %s after '${%.*s' in dsk_print",
                         dsk_ascii_byte_name (*end),
                         len, start);
          return DSK_FALSE;
        }
      *expr_len_out = (end + 1) - expr;
    }
  else if (dsk_ascii_isalnum (expr[1]))
    {
      len = 1;
      start = expr + 1;
      while (dsk_ascii_isalnum (start[len]))
        len++;
      *expr_len_out = len + 1;
    }
  else
    {
      dsk_set_error (error, "unexpected character %s after '$' in dsk_print",
                     dsk_ascii_byte_name (expr[1]));
      return DSK_FALSE;
    }
#define COMPARE_START_LEN_TO_VAR_DEF(unused,b, rv) \
      rv = memcmp (start, b->key, len);            \
      if (rv == 0 && b->key[len] != '\0')          \
        rv = -1;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_VARDEF_TREE (context),
                                unused, COMPARE_START_LEN_TO_VAR_DEF,
                                result);
#undef COMPARE_START_LEN_TO_VAR_DEF
  if (result == NULL)
    {
      dsk_set_error (error,
                     "unset variable $%.*s excountered in print template",
                     len, start);
      return DSK_FALSE;
    }
  if (!context->append (result->value_length, (uint8_t*)(result+1),
                        context->append_data, error))
    return DSK_FALSE;
  return DSK_TRUE;
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
   || !context->append (1, (const uint8_t*) "\n", context->append_data, &error))
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
  char init_slab[1024];
  if (buffer->size == 0)
    {
      dsk_print_set_string (context, variable_name, "");
      return;
    }
  switch (quoting_method)
    {
    case DSK_PRINT_STRING_C_QUOTED:
      {
        DskBufferFragment *frag = buffer->first_frag;
        const uint8_t *frag_at = frag->buf + frag->buf_start;
        const uint8_t *frag_end = frag_at + frag->buf_length;
        uint8_t at = *frag_at++;
        char *slab = init_slab;
        unsigned slab_alloced = sizeof (init_slab);
        unsigned slab_len = 0;
        while (frag != NULL)
          {
            uint8_t next;
            if (frag_at == frag_end)
              {
                frag = frag->next;
                if (frag == NULL)
                  next = 0;
                else
                  {
                    frag_at = frag->buf + frag->buf_start;
                    frag_end = frag_at + frag->buf_length;
                    next = *frag_at++;
                  }
              }
            else
              next = *frag_at++;
            if (slab_len + 5 > slab_alloced)
              {
                if (slab == init_slab)
                  {
                    slab = dsk_malloc (slab_alloced * 2);
                    memcpy (slab, init_slab, slab_len);
                  }
                else
                  slab = dsk_realloc (slab, slab_alloced * 2);
                slab_alloced *= 2;
              }
#define APP(c) slab[slab_len++] = (c)
            switch (at)
              {
              case '\a': APP('\\'); APP('a'); break;
              case '\b': APP('\\'); APP('b'); break;
              case '\f': APP('\\'); APP('f'); break;
              case '\n': APP('\\'); APP('n'); break;
              case '\r': APP('\\'); APP('r'); break;
              case '\t': APP('\\'); APP('t'); break;
              case '\v': APP('\\'); APP('v'); break;
              case '\\': APP('\\'); APP('\\'); break;
              case '"':  APP('\\'); APP('"'); break;
              default:
                if (' ' <= at && at <= 126)
                  APP(at);
                else
                  {
                    if (dsk_ascii_isdigit (next) || at >= 64)
                      {
                        /* three digit codes */
                        APP('\\');
                        APP('0' + at/64);
                        APP('0' + (at/8)%8);
                        APP('0' + at%8);
                      }
                    else if (at >= 8)
                      {
                        /* two digit codes */
                        APP('\\');
                        APP('0' + at/8);
                        APP('0' + at%8);
                      }
                    else
                      {
                        /* one digit code */
                        APP('\\');
                        APP('0' + at);
                      }
                  }
              }
            at = next;
          }
        dsk_print_set_string_length (context, variable_name, slab_len, slab);
        if (slab != init_slab)
          dsk_free (slab);
      }
      break;
    case DSK_PRINT_STRING_HEX:
      {
        unsigned space = buffer->size * 2 + 1;
        DskBufferFragment *frag = buffer->first_frag;
        char *str = space <= sizeof (init_slab) ? init_slab : dsk_malloc (space);
        char *out = str;
        while (frag)
          {
            unsigned len = frag->buf_length;
            uint8_t *at = frag->buf + frag->buf_start;
            while (len--)
              {
                *out++ = dsk_ascii_hex_digits[*at / 16];
                *out++ = dsk_ascii_hex_digits[*at % 16];
              }
            frag = frag->next;
          }
        *out = 0;
        dsk_print_set_string_length (context, variable_name, out - str, str);
        if (str != init_slab)
          dsk_free (str);
      }
      break;
    case DSK_PRINT_STRING_HEX_PAIRS:
      {
        unsigned space = buffer->size * 3;
        DskBufferFragment *frag = buffer->first_frag;
        char *str = space <= sizeof (init_slab) ? init_slab : dsk_malloc (space);
        char *out = str;
        while (frag)
          {
            unsigned len = frag->buf_length;
            uint8_t *at = frag->buf + frag->buf_start;
            while (len--)
              {
                *out++ = dsk_ascii_hex_digits[*at / 16];
                *out++ = dsk_ascii_hex_digits[*at % 16];
                *out++ = ' ';
              }
            frag = frag->next;
          }
        out--;  /* cut off terminal space (note: assumed non-empty string;
                 see above) */
        dsk_print_set_string_length (context, variable_name, out - str, str);
        if (str != init_slab)
          dsk_free (str);
      }
      break;
    case DSK_PRINT_STRING_RAW:
      {
        unsigned space = buffer->size;
        char *str = space <= sizeof (init_slab) ? init_slab : dsk_malloc (space);
        dsk_buffer_peek (buffer, space, str);
        dsk_print_set_string_length (context, variable_name, space, str);
        if (str != init_slab)
          dsk_free (str);
      }
      break;
    case DSK_PRINT_STRING_MYSTERIOUSLY:
      {
        unsigned space = buffer->size;
        char *str = space <= sizeof (init_slab) ? init_slab : dsk_malloc (space);
        unsigned i;
        dsk_buffer_peek (buffer, space, str);
        for (i = 0; i < space; i++)
          if (str[i] < ' ' || ((uint8_t)str[i] >= 127))
            str[i] = '?';
        dsk_print_set_string_length (context, variable_name, space, str);
        if (str != init_slab)
          dsk_free (str);
      }
      break;
    }
}
void dsk_print_set_quoted_string   (DskPrint    *context,
                                    const char  *variable_name,
			            const char  *raw_string,
                                    DskPrintStringQuoting quoting_method)
{
  dsk_print_set_quoted_binary (context, variable_name,
                               strlen (raw_string), raw_string,
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
  frag.buf_length = raw_string_length;
  frag.next = NULL;
  buf.first_frag = buf.last_frag = &frag;
  buf.size = frag.buf_length;
  dsk_print_set_quoted_buffer (context, variable_name, &buf, quoting_method);
}


static dsk_boolean
append_to_file_pointer (unsigned   length,
                        const uint8_t *data,
                        void      *append_data,
                        DskError **error)
{
  /* TODO: use fwrite_unlocked where available */
  if (fwrite (data, length, 1, append_data) != 1)
    {
      dsk_set_error (error, "error writing to file-pointer");
      return DSK_FALSE;
    }
  return DSK_TRUE;
}

static void
fclose_file_pointer (void *data)
{
  fclose (data);
}
static void
pclose_file_pointer (void *data)
{
  pclose (data);
}

DskPrint *dsk_print_new_fp (void *file_pointer)
{
  return dsk_print_new (append_to_file_pointer,
                        file_pointer,
                        NULL);
}
DskPrint *dsk_print_new_fp_fclose (void *file_pointer)
{
  return dsk_print_new (append_to_file_pointer,
                        file_pointer,
                        fclose_file_pointer);
}
DskPrint *dsk_print_new_fp_pclose (void *file_pointer)
{
  return dsk_print_new (append_to_file_pointer,
                        file_pointer,
                        pclose_file_pointer);
}
