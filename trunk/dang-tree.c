#include <string.h>
#include "dang.h"
#include "gskrbtreemacros.h"
#include "magic.h"
#include "config.h"

#define DESTRUCT__none
#define DESTRUCT__k        ttype->key->destruct (ttype->key, node + 1);
#define DESTRUCT__v        ttype->value->destruct (ttype->value, (char*)node + ttype->value_offset);
#define DESTRUCT__kv       DESTRUCT__k DESTRUCT__v
#define COPY__none         memcpy (new_node + 1, node + 1, ttype->node_size - sizeof (DangTreeNode));
#define COPY__k            ttype->key->init_assign (ttype->key, new_node + 1, node + 1); \
                           memcpy ((char*)new_node + ttype->value_offset, \
                                   (char*)node + ttype->value_offset, ttype->value->sizeof_instance);
#define COPY__v            memcpy (new_node + 1, node + 1, ttype->key->sizeof_instance); \
                           ttype->value->init_assign (ttype->value, \
                                                           (char*)new_node + ttype->value_offset, \
                                                           (char*)node + ttype->value_offset);
#define COPY__kv           ttype->key->init_assign (ttype->key, new_node + 1, node + 1); \
                           ttype->value->init_assign (ttype->value, \
                                                           (char*)new_node + ttype->value_offset, \
                                                           (char*)node + ttype->value_offset);
#define IMPLEMENT_TREE_VALUE_FUNCS(suffix)                         \
static void                                                        \
destruct_tree_node__##suffix (DangValueTreeTypes *ttype,            \
                              DangTreeNode      *node)             \
{                                                                  \
  DESTRUCT__##suffix                                               \
  if (node->left)                                                  \
    destruct_tree_node__##suffix (ttype, node->left);              \
  if (node->right)                                                 \
    destruct_tree_node__##suffix (ttype, node->right);             \
  node->parent = ttype->recycling_list;                            \
  ttype->recycling_list = node;                                    \
}                                                                  \
static DangTreeNode *                                              \
copy_tree_node__##suffix (DangValueTreeTypes *ttype,                \
                          DangTreeNode      *parent,               \
                          DangTreeNode      *node)                 \
{                                                                  \
  DangTreeNode *new_node;                                          \
  if (node == NULL)                                                \
    return NULL;                                                   \
  if (ttype->recycling_list)                                       \
    {                                                              \
      new_node = ttype->recycling_list;                            \
      ttype->recycling_list = new_node->parent;                    \
    }                                                              \
  else                                                             \
    new_node = dang_malloc (ttype->node_size);                     \
                                                                   \
  COPY__##suffix                                                   \
  new_node->left = copy_tree_node__##suffix (ttype, new_node, node->left);   \
  new_node->right = copy_tree_node__##suffix (ttype, new_node, node->right); \
  new_node->parent = parent;                                       \
  new_node->is_red = node->is_red;                                 \
  new_node->is_defunct = 0;                                        \
  new_node->visit_count = 0;                                       \
  return new_node;                                                 \
}

IMPLEMENT_TREE_VALUE_FUNCS(none)
IMPLEMENT_TREE_VALUE_FUNCS(k)
IMPLEMENT_TREE_VALUE_FUNCS(v)
IMPLEMENT_TREE_VALUE_FUNCS(kv)

static void
destruct__constant_tree (DangValueType *type,
                         void          *value)
{
  DangConstantTree *ctree = * (DangConstantTree **) value;
  DangValueTreeTypes *tt = ((DangValueTypeTree *) type)->owner;
  if (ctree == NULL)
    return;
  if (--(ctree->ref_count) > 0)
    return;
  if (ctree->top)
    tt->destruct_tree_node (tt, ctree->top);
  if (ctree->compare != NULL)
    dang_function_unref (ctree->compare);
}

static void
destruct__mutable_tree (DangValueType *type,
                         void          *value)
{
  DangTree *tree = * (DangTree **) value;
  DangValueTreeTypes *tt = ((DangValueTypeTree *) type)->owner;
  if (tree == NULL)
    return;
  if (--(tree->ref_count) > 0)
    return;
  destruct__constant_tree (&tt->types[1].base_type, &tree->v);
}

static void
init_assign__constant_tree          (DangValueType *type,
                                     void          *dst,
	                             const void    *src)
{
  DangConstantTree *src_tree = * (DangConstantTree **) src;
  DANG_UNUSED (type);
  if (src_tree)
    src_tree->ref_count++;
  * (DangConstantTree **) dst = src_tree;
}

static void
init_assign__mutable_tree          (DangValueType *type,
                                     void          *dst,
	                             const void    *src)
{
  DangTree *src_tree = * (DangTree **) src;
  DANG_UNUSED (type);
  if (src_tree)
    src_tree->ref_count++;
  * (DangTree **) dst = src_tree;
}

static void
assign__constant_tree         (DangValueType *type,
                               void          *dst,
	                       const void    *src)
{
  destruct__constant_tree (type, dst);
  init_assign__constant_tree (type, dst, src);
}

static void
assign__mutable_tree         (DangValueType *type,
                               void          *dst,
	                       const void    *src)
{
  destruct__mutable_tree (type, dst);
  init_assign__mutable_tree (type, dst, src);
}

static void
append_string_recursive (DangValueTreeTypes *tt,
                         DangTreeNode      *node,
                         DangStringBuffer  *buf)
{
  char *s;
  if (node == NULL)
    return;
  append_string_recursive (tt, node->left, buf);
  if (buf->len > 1)
    dang_string_buffer_append (buf, ", ");
  s = dang_value_to_string (tt->key, node + 1);
  dang_string_buffer_append (buf, s);
  dang_free (s);
  dang_string_buffer_append (buf, " => ");
  s = dang_value_to_string (tt->value, (char*)node + tt->value_offset);
  dang_string_buffer_append (buf, s);
  dang_free (s);
  append_string_recursive (tt, node->right, buf);
}

static char *
to_string__constant_tree (DangValueType *type,
                          const void    *value)
{
  DangConstantTree *tree = * (DangConstantTree **) value;
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  dang_string_buffer_append_c (&buf, '{');
  append_string_recursive (((DangValueTypeTree*)type)->owner, tree->top, &buf);
  dang_string_buffer_append (&buf, " }");
  return buf.str;
}
static char *
to_string__mutable_tree (DangValueType *type,
                          const void    *value)
{
  DangTree *tree = * (DangTree **) value;
  if (tree == NULL)
    return dang_strdup ("(null)");
  else
    return to_string__constant_tree (&((DangValueTypeTree *)type)->owner->types[1].base_type, &tree->v);
}

static dang_boolean
compare_node_keys (DangValueTreeTypes *tt,
               DangConstantTree          *tree,
               const void              *a,
               const void              *b,
               int               *cmp_rv_out,
               DangError        **error)
{
  if (tree->compare != NULL)
    {
      void *values[2] = { (void *) a, (void *) b };
      int32_t rv;
      if (!dang_function_call_nonyielding_v (tree->compare, &rv, values, error))
        return FALSE;
      *cmp_rv_out = rv;
    }
  else
    {
      *cmp_rv_out = tt->key->compare (tt->key, a, b);
    }
  return TRUE;
}

#define GET_NODE_IS_RED(n)   n->is_red
#define SET_NODE_IS_RED(n,v)   n->is_red=v
#define COMPARE_NODES(a,b,rv) \
{ if (!compare_node_keys (tt, tree, a + 1, b + 1, &rv, error)) \
    return FALSE; }
#define GET_TREE(ttype, tree) \
  tree->top, DangTreeNode *, GET_NODE_IS_RED, SET_NODE_IS_RED, parent, left, right, \
  COMPARE_NODES

#define COMPARE_TREE_TYPES(a,b,rv)              \
  if (a->key < b->key) rv = -1;                 \
  else if (a->key > b->key) rv = 1;             \
  else if (a->value < b->value) rv = -1;        \
  else if (a->value > b->value) rv = 1;         \
  else rv = 0;
static DangValueTreeTypes *top_tree_type = NULL;
#define GET_TREE_TYPE_TREE() \
        top_tree_type, DangValueTreeTypes *, GET_NODE_IS_RED, SET_NODE_IS_RED, \
        parent, left, right, COMPARE_TREE_TYPES

static dang_boolean
constant_tree_get_pointer   (DangValueTreeTypes *tt,
                             DangConstantTree   **ptree,
                             const void         *key,
                             void         **rv_ptr_out,
                             dang_boolean   may_create,
                             DangError    **error)
{
  DangConstantTree *tree = *ptree;
  if (tree == NULL)
    {
      if (may_create)
        {
          tree = dang_new (DangConstantTree, 1);
          tree->ref_count = 1;
          tree->top = NULL;
          tree->compare = NULL;
          tree->size = 0;
          *ptree = tree;
        }
      else
        {
          if (error)
            *error = dang_error_new ("key not found in tree");
          return FALSE;
        }
    }
  DangTreeNode *n = tree->top;
  DangTreeNode *last = n;
  int cmp;
  while (n != NULL)
    {
      if (!compare_node_keys (tt, tree, key, n + 1, &cmp, error))
        return FALSE;
      last = n;
      if (cmp == 0)
        {
          *rv_ptr_out = ((char*)n) + tt->value_offset;
          return TRUE;
        }
      else if (cmp < 0)
        n = n->left;
      else
        n = n->right;
    }
  if (!may_create)
    {
      if (error)
        *error = dang_error_new ("key not found in tree");
      return FALSE;
    }

  if (tt->recycling_list)
    {
      n = tt->recycling_list;
      tt->recycling_list = n->parent;
    }
  else
    n = dang_malloc (tt->node_size);

  /* Initialize key/value */
  dang_value_init_assign (tt->key, n+1, key);
  memset ((char*)n + tt->value_offset, 0, tt->value->sizeof_instance);

  if (last == NULL)
    {
      DangTreeNode *conflict;
      GSK_RBTREE_INSERT (GET_TREE (tt, tree), n, conflict);
      dang_assert (conflict == NULL);
    }
  else
    {
      dang_boolean is_right = (cmp > 0);
      GSK_RBTREE_INSERT_AT (GET_TREE (tt, tree), last, is_right, n);
    }
  *rv_ptr_out = ((char*)n) + tt->value_offset;
  return TRUE;
}

static dang_boolean
index_set__mutable_tree   (DangValueIndexInfo *info,
                   void          *container,
                   const void   **indices,
                   const void    *element_value,
                   dang_boolean   may_create,
                   DangError    **error)
{
  DangValueTypeTree *ttype = (DangValueTypeTree *) (info->owner);
  DangValueTreeTypes *tt = ttype->owner;
  DangTree *tree = * (DangTree **) container;
  void *value_ptr;
  if (tree == NULL)
    {
      dang_set_error (error, "null pointer exception");
      return FALSE;
    }
  if (tree->v->ref_count > 1)
    {
      /* copy constant tree */
      DangConstantTree *ctree = dang_new (DangConstantTree, 1);
      ctree->ref_count = 1;
      ctree->top = tt->copy_tree_node (tt, NULL, tree->v->top);
      ctree->compare = tree->v->compare ? dang_function_ref (tree->v->compare) : NULL;
      ctree->size = tree->v->size;
      tree->v->ref_count -= 1;
      tree->v = ctree;
    }
  if (!constant_tree_get_pointer (tt, &tree->v, indices[0], &value_ptr, may_create, error))
    return FALSE;
  dang_value_assign (tt->value, value_ptr, element_value);
  return TRUE;
}

static dang_boolean
index_get__mutable_tree (DangValueIndexInfo *info,
                         void          *container,
                         const void   **indices,
                         void          *rv_out,
                         dang_boolean   may_create,
                         DangError    **error)
{
  DangTree *tree = * (DangTree **) container;
  DangValueTypeTree *ttype = (DangValueTypeTree *)info->owner;
  DangValueTreeTypes *tt = ttype->owner;
  void *value_ptr;
  if (tree == NULL)
    {
      dang_set_error (error, "null pointer exception");
      return FALSE;
    }
  if (!constant_tree_get_pointer (tt, &tree->v, indices[0], &value_ptr, may_create, error))
    return FALSE;
  dang_value_assign (tt->value, rv_out, value_ptr);
  return TRUE;
}

static dang_boolean
index_get__constant_tree (DangValueIndexInfo *info,
                         void          *container,
                         const void   **indices,
                         void          *rv_out,
                         dang_boolean   may_create,
                         DangError    **error)
{
  DangValueTypeTree *ttype = (DangValueTypeTree *)info->owner;
  DangValueTreeTypes *tt = ttype->owner;
  void *value_ptr;
  if (!constant_tree_get_pointer (tt, container, indices[0], &value_ptr, may_create, error))
    return FALSE;
  dang_value_assign (tt->value, rv_out, value_ptr);
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE (constant_tree_to_mutable_tree)
{
  DangConstantTree *in = * (DangConstantTree **) args[0];
  DangTree *out;
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  if (in)
    in->ref_count += 1;
  out = dang_new (DangTree, 1);
  out->ref_count = 1;
  out->v = in;
  * (DangTree **) rv_out = out;
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE (construct_empty_mutable_tree)
{
  DangTree *tree = dang_new (DangTree, 1);
  DANG_UNUSED (func_data);
  DANG_UNUSED (error);
  DANG_UNUSED (args);
  tree->ref_count = 1;
  tree->v = dang_new (DangConstantTree, 1);
  tree->v->ref_count = 1;
  tree->v->top = NULL;
  * (DangTree **) rv_out = tree;
  return TRUE;
}
static DangValueTreeTypes *
dang_value_tree_types (DangValueType *key,
                       DangValueType *value)
{
  DangValueTreeTypes *rv;
  DangValueTreeTypes dummy;
  DangValueTreeTypes *conflict;
  unsigned align;
  unsigned i;
  DangFunctionParam params[5];
  dummy.key = key;
  dummy.value = value;
  GSK_RBTREE_LOOKUP (GET_TREE_TYPE_TREE (), &dummy, rv);
  if (rv)
    return rv;

  rv = dang_new0 (DangValueTreeTypes, 1);

  rv->key = key;
  rv->value = value;
  rv->value_offset = sizeof (DangTreeNode) + key->sizeof_instance;
  rv->value_offset = DANG_ALIGN (rv->value_offset, value->alignof_instance);
  rv->node_size = rv->value_offset + value->sizeof_instance;
  align = DANG_MAX (DANG_ALIGNOF_POINTER, key->alignof_instance);
  align = DANG_MAX (align, value->alignof_instance);
  rv->node_size = DANG_ALIGN (rv->node_size, align);

  for (i = 0; i < 2; i++)
    {
      rv->types[i].base_type.magic = DANG_VALUE_TYPE_MAGIC;
      rv->types[i].base_type.ref_count = 1;
      rv->types[i].base_type.full_name = dang_strdup_printf ("tree<%s,%s>", key->full_name, value->full_name);
      rv->types[i].base_type.sizeof_instance = sizeof (void*);
      rv->types[i].base_type.alignof_instance = DANG_ALIGNOF_POINTER;
      rv->types[i].owner = rv;
      rv->types[i].base_type.internals.index_infos = &rv->types[i].index_info;
      rv->types[i].index_info.owner = &rv->types[i].base_type;
      rv->types[i].index_info.n_indices = 1;
      rv->types[i].index_info.indices = &rv->key;
      rv->types[i].index_info.element_type = value;
    }

  rv->types[0].base_type.init_assign = init_assign__mutable_tree;
  rv->types[0].base_type.assign = assign__mutable_tree;
  rv->types[0].base_type.destruct = destruct__mutable_tree;
  rv->types[0].base_type.to_string = to_string__mutable_tree;
  rv->types[0].index_info.get = index_get__mutable_tree;
  rv->types[0].index_info.set = index_set__mutable_tree;
  rv->types[0].index_info.next = NULL;
  rv->types[1].base_type.init_assign = init_assign__constant_tree;
  rv->types[1].base_type.assign = assign__constant_tree;
  rv->types[1].base_type.destruct = destruct__constant_tree;
  rv->types[1].base_type.to_string = to_string__constant_tree;
  rv->types[1].index_info.get = index_get__constant_tree;
  rv->types[1].index_info.set = NULL;
  rv->types[1].index_info.next = NULL;

  GSK_RBTREE_INSERT (GET_TREE_TYPE_TREE (), rv, conflict);
  dang_assert (conflict == NULL);

  dang_value_type_add_simple_member (&rv->types[0].base_type,
                                     "v",
                                     DANG_MEMBER_PUBLIC_READABLE,
                                     &rv->types[1].base_type,
                                     TRUE,
                                     offsetof (DangArray, tensor));

  /* make_tree() function */
  params[0].type = &rv->types[1].base_type;
  params[0].name = "this";
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  DangSignature *sig;
  DangFunction *func;
  sig = dang_signature_new (&rv->types[0].base_type,
                            1, params);
  func = dang_function_new_simple_c (sig, constant_tree_to_mutable_tree, NULL, NULL);
  dang_value_type_add_constant_method ((DangValueType *) &rv->types[0].base_type,
                                       "make_tree",
                                       DANG_METHOD_FINAL|DANG_METHOD_PUBLIC,
                                       func);
  dang_function_unref (func);
  dang_signature_unref (sig);


  sig = dang_signature_new (&rv->types[0].base_type, 0, NULL);
  func = dang_function_new_simple_c (sig, construct_empty_mutable_tree, NULL, NULL);
  dang_value_type_add_constant_method ((DangValueType *) &rv->types[0].base_type,
                                       "make_empty",
                                       DANG_METHOD_FINAL|DANG_METHOD_PUBLIC|DANG_METHOD_STATIC,
                                       func);
  dang_signature_unref (sig);
  dang_function_unref (func);

  if (key->init_assign)
    {
      if (value->init_assign)
        {
          rv->copy_tree_node = copy_tree_node__kv;
          rv->destruct_tree_node = destruct_tree_node__kv;
        }
      else
        {
          rv->copy_tree_node = copy_tree_node__k;
          rv->destruct_tree_node = destruct_tree_node__k;
        }
    }
  else
    {
      if (value->init_assign)
        {
          rv->copy_tree_node = copy_tree_node__v;
          rv->destruct_tree_node = destruct_tree_node__v;
        }
      else
        {
          rv->copy_tree_node = copy_tree_node__none;
          rv->destruct_tree_node = destruct_tree_node__none;
        }
    }
  return rv;
}


DangValueType *
dang_value_type_tree (DangValueType *key,
                      DangValueType *value)
{
  return &dang_value_tree_types (key, value)->types[0].base_type;
}
DangValueType *
dang_value_type_constant_tree (DangValueType *key,
                               DangValueType *value)
{
  return &dang_value_tree_types (key, value)->types[1].base_type;
}
