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
destruct_tree_node__##suffix (DangValueTypeTree *ttype,            \
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
copy_tree_node__##suffix (DangValueTypeTree *ttype,                \
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
destruct__tree (DangValueType *type,
                void          *value)
{
  DangValueTypeTree *ttype = (DangValueTypeTree *) type;
  DangTree *tree = value;
  if (tree->top)
    ttype->destruct_tree_node (ttype, tree->top);
  if (tree->compare != NULL)
    dang_function_unref (tree->compare);
}

static void
init_assign__tree          (DangValueType *type,
                            void          *dst,
	                    const void    *src)
{
  DangValueTypeTree *ttype = (DangValueTypeTree *) type;
  DangTree *dst_tree = dst;
  DangTree *src_tree = (DangTree*) src;
  dst_tree->top = ttype->copy_tree_node (ttype, NULL, src_tree->top);
  dst_tree->size = src_tree->size;
  dst_tree->compare = src_tree->compare ? dang_function_ref (src_tree->compare) : NULL;
}

static void
assign__tree         (DangValueType *type,
                      void          *dst,
	              const void    *src)
{
  destruct__tree (type, dst);
  init_assign__tree (type, dst, src);
}

static void
append_string_recursive (DangValueTypeTree *ttype,
                         DangTreeNode      *node,
                         DangStringBuffer  *buf)
{
  char *s;
  if (node == NULL)
    return;
  append_string_recursive (ttype, node->left, buf);
  if (buf->len > 1)
    dang_string_buffer_append (buf, ", ");
  s = dang_value_to_string (ttype->key, node + 1);
  dang_string_buffer_append (buf, s);
  dang_free (s);
  dang_string_buffer_append (buf, " => ");
  s = dang_value_to_string (ttype->value, (char*)node + ttype->value_offset);
  dang_string_buffer_append (buf, s);
  dang_free (s);
  append_string_recursive (ttype, node->right, buf);
}

static char *
to_string__tree (DangValueType *type,
                 const void    *value)
{
  DangTree *tree = (DangTree *) value;
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  dang_string_buffer_append_c (&buf, '{');
  append_string_recursive ((DangValueTypeTree*)type, tree->top, &buf);
  dang_string_buffer_append (&buf, " }");
  return buf.str;
}

static dang_boolean
compare_node_keys (DangValueTypeTree *ttype,
               DangTree          *tree,
               void              *a,
               void              *b,
               int               *cmp_rv_out,
               DangError        **error)
{
  if (tree->compare != NULL)
    {
      void *values[2] = { a, b };
      int32_t rv;
      if (!dang_function_call_nonyielding_v (tree->compare, &rv, values, error))
        return FALSE;
      *cmp_rv_out = rv;
    }
  else
    {
      *cmp_rv_out = ttype->key->compare (ttype->key, a, b);
    }
  return TRUE;
}

#define GET_NODE_IS_RED(n)   n->is_red
#define SET_NODE_IS_RED(n,v)   n->is_red=v
#define COMPARE_NODES(a,b,rv) \
{ if (!compare_node_keys (ttype, tree, a + 1, b + 1, &rv, error)) \
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
static DangValueTypeTree *top_tree_type = NULL;
#define GET_TREE_TYPE_TREE() \
        top_tree_type, DangValueTypeTree *, GET_NODE_IS_RED, SET_NODE_IS_RED, \
        parent, left, right, COMPARE_TREE_TYPES

static dang_boolean
index_get_ptr__tree   (DangValueIndexInfo *info,
                   void          *container,
                   const void   **indices,
                   void         **rv_ptr_out,
                   dang_boolean   may_create,
                   DangError    **error)
{
  DangValueTypeTree *ttype = (DangValueTypeTree *) info->owner;
  DangTree *tree = container;
  DangTreeNode *n = tree->top;
  DangTreeNode *last = n;
  void *key = (void*) (indices[0]);
  int cmp;
  while (n != NULL)
    {
      if (!compare_node_keys (ttype, tree, key, n + 1, &cmp, error))
        return FALSE;
      last = n;
      if (cmp == 0)
        {
          *rv_ptr_out = ((char*)n) + ttype->value_offset;
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

  if (ttype->recycling_list)
    {
      n = ttype->recycling_list;
      ttype->recycling_list = n->parent;
    }
  else
    n = dang_malloc (ttype->node_size);

  /* Initialize key/value */
  dang_value_init_assign (ttype->key, n+1, indices[0]);
  memset ((char*)n + ttype->value_offset, 0, ttype->value->sizeof_instance);

  if (last == NULL)
    {
      DangTreeNode *conflict;
      GSK_RBTREE_INSERT (GET_TREE (ttype, tree), n, conflict);
      dang_assert (conflict == NULL);
    }
  else
    {
      dang_boolean is_right = (cmp > 0);
      GSK_RBTREE_INSERT_AT (GET_TREE (ttype, tree), last, is_right, n);
    }
  *rv_ptr_out = ((char*)n) + ttype->value_offset;
  return TRUE;
}

static dang_boolean
index_set__tree   (DangValueIndexInfo *info,
                   void          *container,
                   const void   **indices,
                   const void    *element_value,
                   dang_boolean   may_create,
                   DangError    **error)
{
  void *ptr;
  if (!index_get_ptr__tree (info, container, indices, &ptr, may_create, error))
    return FALSE;
  dang_value_assign (info->element_type, ptr, element_value);
  return TRUE;
}

static dang_boolean
index_get__tree (DangValueIndexInfo *info,
                     void          *container,
                     const void   **indices,
                     void          *rv_out,
                     dang_boolean   may_create,
                     DangError    **error)
{
  void *ptr;
  if (!index_get_ptr__tree (info, container, indices, &ptr, may_create, error))
    return FALSE;
  dang_value_init_assign (info->element_type, rv_out, ptr);
  return TRUE;
}

DangValueType *
dang_value_type_tree (DangValueType *key,
                      DangValueType *value)
{
  DangValueTypeTree *rv;
  DangValueTypeTree dummy;
  DangValueTypeTree *conflict;
  unsigned align;
  dummy.key = key;
  dummy.value = value;
  GSK_RBTREE_LOOKUP (GET_TREE_TYPE_TREE (), &dummy, rv);
  if (rv)
    return &rv->base_type;

  rv = dang_new0 (DangValueTypeTree, 1);
  rv->base_type.magic = DANG_VALUE_TYPE_MAGIC;
  rv->base_type.ref_count = 1;
  rv->base_type.full_name = dang_strdup_printf ("tree<%s,%s>", key->full_name, value->full_name);
  rv->base_type.sizeof_instance = sizeof (DangTree);
  rv->base_type.alignof_instance = DANG_ALIGNOF_POINTER;
  rv->base_type.init_assign = init_assign__tree;
  rv->base_type.assign = assign__tree;
  rv->base_type.destruct = destruct__tree;
  rv->base_type.to_string = to_string__tree;
  rv->base_type.internals.index_infos = &rv->index_info;
  rv->index_info.owner = &rv->base_type;
  rv->index_info.n_indices = 1;
  rv->index_info.indices = &rv->value;
  rv->index_info.element_type = value;
  rv->index_info.get = index_get__tree;
  rv->index_info.set = index_set__tree;
  rv->index_info.get_ptr = index_get_ptr__tree;
  rv->index_info.next = NULL;
  rv->key = key;
  rv->value = value;
  rv->value_offset = sizeof (DangTreeNode) + key->sizeof_instance;
  rv->value_offset = DANG_ALIGN (rv->value_offset, value->alignof_instance);
  rv->node_size = rv->value_offset + value->sizeof_instance;
  align = DANG_MAX (DANG_ALIGNOF_POINTER, key->alignof_instance);
  align = DANG_MAX (align, value->alignof_instance);
  rv->node_size = DANG_ALIGN (rv->node_size, align);
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
  GSK_RBTREE_INSERT (GET_TREE_TYPE_TREE (), rv, conflict);
  dang_assert (conflict == NULL);
  return &rv->base_type;
}
