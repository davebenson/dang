#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "dsk.h"
#include "dsk-xml-binding-internals.h"
#include "../gskrbtreemacros.h"
#define DSK_STRUCT_MEMBER_P(struct_p, struct_offset)   \
    ((void*) ((char*) (struct_p) + (long) (struct_offset)))
#define DSK_STRUCT_MEMBER(member_type, struct_p, struct_offset)   \
    (*(member_type*) DSK_STRUCT_MEMBER_P ((struct_p), (struct_offset)))

typedef struct _NamespaceNode NamespaceNode;
struct _NamespaceNode
{
  DskXmlBindingNamespace *ns;
  NamespaceNode *left, *right, *parent;
  dsk_boolean is_red;
};
#define COMPARE_NAMESPACE_NODES(a,b, rv) rv = strcmp (a->ns->name, b->ns->name)
#define NAMESPACE_TREE(binding) \
  binding->namespace_tree, NamespaceNode*, \
  GSK_STD_GET_IS_RED, GSK_STD_SET_IS_RED, \
  parent, left, right, \
  COMPARE_NAMESPACE_NODES

#define COMPARE_STRING_TO_NAMESPACE_INFO(a, b, rv) rv = strcmp (a, b->ns->name)

typedef struct _SearchPath SearchPath;
struct _SearchPath
{
  char *path;
  unsigned path_len;
  char *separator;
  unsigned sep_len;
};
struct _DskXmlBinding
{
  NamespaceNode *namespace_tree;

  unsigned n_search_paths;
  SearchPath *search_paths;
};

DskXmlBinding *dsk_xml_binding_new (void)
{
  DskXmlBinding *rv = dsk_malloc (sizeof (DskXmlBinding));
  rv->namespace_tree = NULL;
  rv->n_search_paths = 0;
  rv->search_paths = NULL;
  return rv;
}
void           dsk_xml_binding_add_searchpath (DskXmlBinding *binding,
                                               const char    *path,
                                               const char    *ns_separator)
{
  binding->search_paths = dsk_realloc (binding->search_paths,
                                       (binding->n_search_paths+1) * sizeof (SearchPath));
  binding->search_paths[binding->n_search_paths].path = dsk_strdup (path);
  binding->search_paths[binding->n_search_paths].separator = dsk_strdup (ns_separator);
  binding->search_paths[binding->n_search_paths].path_len = strlen (path);
  binding->search_paths[binding->n_search_paths].sep_len = strlen (ns_separator);
  binding->n_search_paths++;
}

DskXmlBindingNamespace*
dsk_xml_binding_try_ns         (DskXmlBinding *binding,
                                const char    *name)
{
  NamespaceNode *ns;
  GSK_RBTREE_LOOKUP_COMPARATOR (NAMESPACE_TREE (binding),
                                name, COMPARE_STRING_TO_NAMESPACE_INFO,
                                ns);
  return ns ? ns->ns : NULL;
}

DskXmlBindingNamespace*
dsk_xml_binding_get_ns         (DskXmlBinding *binding,
                                const char    *name,
                                DskError     **error)
{
  DskXmlBindingNamespace *ns = dsk_xml_binding_try_ns (binding, name);
  unsigned n_dots, n_non_dots;
  unsigned i;
  const char *at;
  if (ns != NULL)
    return ns;

  n_dots = n_non_dots = 0;
  for (at = name; *at; at++)
    if (*at == '.')
      n_dots++;
    else
      n_non_dots++;

  /* look along searchpath */
  for (i = 0; i < binding->n_search_paths; i++)
    {
      unsigned len = binding->search_paths[i].path_len
                   + 1
                   + n_non_dots
                   + n_dots * binding->search_paths[i].sep_len
                   + 1;
      char *path = dsk_malloc (len);
      char *out = path + binding->search_paths[i].path_len;
      memcpy (path, binding->search_paths[i].path,
              binding->search_paths[i].path_len);
      *out++ = '/';
      for (at = name; *at; at++)
        if (*at == '.')
          {
            memcpy (out, binding->search_paths[i].separator,
                    binding->search_paths[i].sep_len);
            out += binding->search_paths[i].sep_len;
          }
        else
          *out++ = *at;
      *out = 0;

      /* try opening file */
      if (dsk_file_test_exists (path))
        {
          char *contents = dsk_file_get_contents (path, NULL, error);
          DskXmlBindingNamespace *real_ns;
          if (contents == NULL)
            {
              dsk_free (path);
              return NULL;
            }
          real_ns = _dsk_xml_binding_parse_ns_str (binding, path, name,
                                                   contents, error);
          if (real_ns == NULL)
            {
              dsk_free (path);
              return NULL;
            }
          dsk_free (path);

          /* add to tree */
          NamespaceNode *node;
          NamespaceNode *conflict;
          node = dsk_malloc (sizeof (NamespaceNode));
          node->ns = real_ns;
          GSK_RBTREE_INSERT (NAMESPACE_TREE (binding), node, conflict);
          dsk_assert (conflict == NULL);
          return real_ns;
        }
      dsk_free (path);
    }

  dsk_set_error (error, "namespace %s could not be found on search-path", name);
  return NULL;
}

/* --- fundamental types --- */
#define CHECK_IS_TEXT() \
  if (to_parse->type != DSK_XML_TEXT) \
    { \
      dsk_set_error (error, "expected string XML node for type %s, got <%s>", \
                     type->name, to_parse->str); \
      return DSK_FALSE; \
    }
static dsk_boolean
xml_binding_parse__int(DskXmlBindingType *type,
                       DskXml            *to_parse,
		       void              *out,
		       DskError         **error)
{
  char *end;
  CHECK_IS_TEXT ();
  * (int *) out = strtol (to_parse->str, &end, 10);
  if (end == to_parse->str || *end != 0)
    {
      dsk_set_error (error, "error parsing int from XML node");
      return DSK_FALSE;
    }
  return DSK_TRUE;
}

static DskXml   *
xml_binding_to_xml__int(DskXmlBindingType *type,
                        const void        *data,
		        DskError         **error)
{
  char buf[64];
  DSK_UNUSED (type); DSK_UNUSED (error);
  snprintf (buf, sizeof (buf), "%d", * (int *) data);
  return dsk_xml_text_new (buf);
}

DskXmlBindingType dsk_xml_binding_type_int =
{
  DSK_TRUE,    /* is_fundamental */
  DSK_TRUE,    /* is_static */
  DSK_FALSE,   /* is_struct */
  DSK_FALSE,   /* is_union */

  sizeof (int),
  DSK_ALIGNOF_INT,

  NULL,        /* no namespace */
  "int",       /* name */
  xml_binding_parse__int,
  xml_binding_to_xml__int,
  NULL         /* no clear */
};

static dsk_boolean
xml_binding_parse__uint(DskXmlBindingType *type,
                        DskXml            *to_parse,
		        void              *out,
		        DskError         **error)
{
  char *end;
  CHECK_IS_TEXT ();
  * (int *) out = strtoul (to_parse->str, &end, 10);
  if (end == to_parse->str || *end != 0)
    {
      dsk_set_error (error, "error parsing uint from XML node");
      return DSK_FALSE;
    }
  return DSK_TRUE;
}

static DskXml   *
xml_binding_to_xml__uint(DskXmlBindingType *type,
                         const void        *data,
		         DskError         **error)
{
  char buf[64];
  DSK_UNUSED (type); DSK_UNUSED (error);
  snprintf (buf, sizeof (buf), "%u", * (unsigned int *) data);
  return dsk_xml_text_new (buf);
}

DskXmlBindingType dsk_xml_binding_type_uint = 
{
  DSK_TRUE,    /* is_fundamental */
  DSK_TRUE,    /* is_static */
  DSK_FALSE,   /* is_struct */
  DSK_FALSE,   /* is_union */

  sizeof (unsigned int),
  DSK_ALIGNOF_INT,

  NULL,        /* no namespace */
  "uint",      /* name */
  xml_binding_parse__uint,
  xml_binding_to_xml__uint,
  NULL         /* no clear */
};

static dsk_boolean
xml_binding_parse__float(DskXmlBindingType *type,
                        DskXml            *to_parse,
		        void              *out,
		        DskError         **error)
{
  char *end;
  CHECK_IS_TEXT ();
  * (float *) out = strtod (to_parse->str, &end);
  if (end == to_parse->str || *end != 0)
    {
      dsk_set_error (error, "error parsing float from XML node");
      return DSK_FALSE;
    }
  return DSK_TRUE;
}

static DskXml   *
xml_binding_to_xml__float(DskXmlBindingType *type,
                          const void        *data,
		          DskError         **error)
{
  char buf[64];
  DSK_UNUSED (type); DSK_UNUSED (error);
  snprintf (buf, sizeof (buf), "%.6f", * (float *) data);
  return dsk_xml_text_new (buf);
}

DskXmlBindingType dsk_xml_binding_type_float = 
{
  DSK_TRUE,    /* is_fundamental */
  DSK_TRUE,    /* is_static */
  DSK_FALSE,   /* is_struct */
  DSK_FALSE,   /* is_union */

  sizeof (float),
  DSK_ALIGNOF_FLOAT,

  NULL,        /* no namespace */
  "float",      /* name */
  xml_binding_parse__float,
  xml_binding_to_xml__float,
  NULL         /* no clear */
};

static dsk_boolean
xml_binding_parse__double(DskXmlBindingType *type,
                        DskXml            *to_parse,
		        void              *out,
		        DskError         **error)
{
  char *end;
  CHECK_IS_TEXT ();
  * (double *) out = strtod (to_parse->str, &end);
  if (end == to_parse->str || *end != 0)
    {
      dsk_set_error (error, "error parsing double from XML node");
      return DSK_FALSE;
    }
  return DSK_TRUE;
}

static DskXml   *
xml_binding_to_xml__double(DskXmlBindingType *type,
                          const void        *data,
		          DskError         **error)
{
  char buf[64];
  DSK_UNUSED (type); DSK_UNUSED (error);
  snprintf (buf, sizeof (buf), "%.16f", * (double *) data);
  return dsk_xml_text_new (buf);
}

DskXmlBindingType dsk_xml_binding_type_double = 
{
  DSK_TRUE,    /* is_fundamental */
  DSK_TRUE,    /* is_static */
  DSK_FALSE,   /* is_struct */
  DSK_FALSE,   /* is_union */

  sizeof (double),
  DSK_ALIGNOF_DOUBLE,

  NULL,        /* no namespace */
  "double",    /* name */
  xml_binding_parse__double,
  xml_binding_to_xml__double,
  NULL         /* no clear */
};

static dsk_boolean
xml_binding_parse__string(DskXmlBindingType *type,
                          DskXml            *to_parse,
		          void              *out,
		          DskError         **error)
{
  CHECK_IS_TEXT ();
  * (char **) out = dsk_strdup (to_parse->str);
  return DSK_TRUE;
}

static DskXml   *
xml_binding_to_xml__string(DskXmlBindingType *type,
                           const void        *data,
		           DskError         **error)
{
  const char *str = * (char **) data;
  DSK_UNUSED (type); DSK_UNUSED (error);
  return dsk_xml_text_new (str ? str : "");
}
static void
clear__string(DskXmlBindingType *type,
              void              *data)
{
  DSK_UNUSED (type);
  dsk_free (* (char **) data);
}

DskXmlBindingType dsk_xml_binding_type_string = 
{
  DSK_TRUE,    /* is_fundamental */
  DSK_TRUE,    /* is_static */
  DSK_FALSE,   /* is_struct */
  DSK_FALSE,   /* is_union */

  sizeof (char *),
  DSK_ALIGNOF_POINTER,

  NULL,        /* no namespace */
  "string",    /* name */
  xml_binding_parse__string,
  xml_binding_to_xml__string,
  clear__string
};
static dsk_boolean xml_is_whitespace (DskXml *xml)
{
  if (xml->type == DSK_XML_ELEMENT)
    return DSK_FALSE;
  const char *at;
  at = xml->str;
  while (*at)
    {
      if (!dsk_ascii_isspace (*at))
        return DSK_FALSE;
      at++;
    }
  return DSK_TRUE;
}

static dsk_boolean
is_value_bearing_node (DskXml *child, DskError **error)
{
  unsigned i;
  unsigned n_nonws = 0;
  dsk_assert (child->type == DSK_XML_ELEMENT);
  if (child->n_children <= 1)
    return DSK_TRUE;
  for (i = 0; i < child->n_children; i++)
    if (!xml_is_whitespace (child->children[i]))
      n_nonws++;
  if (n_nonws > 1)
    {
      dsk_set_error (error, "<%s> contains multiple values", child->str);
      return DSK_FALSE;
    }
  return DSK_TRUE;
}
static DskXml *
get_value_from_value_bearing_node (DskXml *child)
{
  if (child->n_children == 0)
    return dsk_xml_empty_text;
  else if (child->n_children == 1)
    return child->children[0];
  else
    {
      unsigned i;
      for (i = 0; i < child->n_children; i++)
        if (!xml_is_whitespace (child->children[i]))
          return child->children[i];
    }
  return NULL;  /* can't happen since is_value_bearing_node() returned TRUE */
}

/* structures */
dsk_boolean
parse_struct_ignore_outer_tag  (DskXmlBindingType *type,
                                DskXml            *to_parse,
                                void              *out,
                                DskError         **error)
{
  DskXmlBindingTypeStruct *s = (DskXmlBindingTypeStruct *) type;
  unsigned *counts;
  unsigned *member_index;
  unsigned i;
  if (to_parse->type != DSK_XML_ELEMENT)
    {
      dsk_set_error (error, "cannot parse structure from string");
      return DSK_FALSE;
    }
  counts = alloca (s->n_members * sizeof (unsigned));
  for (i = 0; i < s->n_members; i++)
    counts[i] = 0;
  member_index = alloca (to_parse->n_children * sizeof (unsigned));
  for (i = 0; i < to_parse->n_children; i++)
    if (to_parse->children[i]->type == DSK_XML_ELEMENT)
      {
        const char *mem_name = to_parse->children[i]->str;
        int mem_no = dsk_xml_binding_type_struct_lookup_member (s, mem_name);
        if (mem_no < 0)
          {
            dsk_set_error (error, "no member %s found", mem_name);
            return DSK_FALSE;
          }
        member_index[i] = mem_no;
        counts[mem_no] += 1;

        if (!is_value_bearing_node (to_parse->children[i], error))
          return DSK_FALSE;
      }
  for (i = 0; i < s->n_members; i++)
    switch (s->members[i].quantity)
      {
      case DSK_XML_BINDING_REQUIRED:
        if (counts[i] == 0)
          {
            dsk_set_error (error, "required member %s not found",
                           s->members[i].name);
            return DSK_FALSE;
          }
        else if (counts[i] > 1)
          {
            dsk_set_error (error, "required member %s specified more than once",
                           s->members[i].name);
            return DSK_FALSE;
          }
        break;
      case DSK_XML_BINDING_OPTIONAL:
        if (counts[i] > 1)
          {
            dsk_set_error (error, "optional member %s specified more than once",
                           s->members[i].name);
            return DSK_FALSE;
          }
        DSK_STRUCT_MEMBER (unsigned, out,
                           s->members[i].quantifier_offset) = counts[i];
        break;
      case DSK_XML_BINDING_REQUIRED_REPEATED:
        if (counts[i] == 0)
          {
            dsk_set_error (error, "repeated required member %s not found",
                           s->members[i].name);
            return DSK_FALSE;
          }
        break;
      case DSK_XML_BINDING_REPEATED:
        break;
      }
  out = dsk_malloc (type->sizeof_instance);
  for (i = 0; i < s->n_members; i++)
    switch (s->members[i].quantity)
      {
      case DSK_XML_BINDING_REQUIRED:
        break;
      case DSK_XML_BINDING_OPTIONAL:
        DSK_STRUCT_MEMBER (unsigned, out,
                           s->members[i].quantifier_offset) = counts[i];
        break;
      case DSK_XML_BINDING_REQUIRED_REPEATED:
      case DSK_XML_BINDING_REPEATED:
        DSK_STRUCT_MEMBER (void *, out, s->members[i].offset)
          = dsk_malloc (s->members[i].type->sizeof_instance * counts[i]);
        break;
      }
  void *strct;
  strct = dsk_malloc0 (s->sizeof_struct);
  for (i = 0; i < to_parse->n_children; i++)
    if (to_parse->children[i]->type == DSK_XML_ELEMENT)
      {
        unsigned index = member_index[i];
        DskXmlBindingStructMember *member = s->members + index;
        DskXml *value_xml = get_value_from_value_bearing_node (to_parse->children[i]);
        switch (member->quantity)
          {
          case DSK_XML_BINDING_REQUIRED:
          case DSK_XML_BINDING_OPTIONAL:
            if (!member->type->parse (member->type, value_xml,
                                      DSK_STRUCT_MEMBER_P (strct, member->offset),
                                      error))
              {
                /* TO CONSIDER: add error info */
                goto error_cleanup;
              }
            break;
          case DSK_XML_BINDING_REPEATED:
          case DSK_XML_BINDING_REQUIRED_REPEATED:
            {
              unsigned *p_idx = DSK_STRUCT_MEMBER_P (strct, member->quantifier_offset);
              unsigned idx = *p_idx;
              char *slab = DSK_STRUCT_MEMBER (void *, strct, member->offset);
              if (!member->type->parse (member->type, value_xml,
                                        slab + member->type->sizeof_instance * idx,
                                        error))
                {
                  /* TO CONSIDER: add error info */
                  goto error_cleanup;
                }
              *p_idx += 1;
            }
            break;
          }
      }
  * (void **) out = strct;
  return DSK_TRUE;

error_cleanup:
  {
    unsigned j, k;
    for (j = 0; j < i; j++)
      if (to_parse->children[j]->type == DSK_XML_ELEMENT
       && (s->members[member_index[j]].quantity == DSK_XML_BINDING_REQUIRED
        || s->members[member_index[j]].quantity == DSK_XML_BINDING_OPTIONAL))
        {
          DskXmlBindingType *mtype = s->members[member_index[j]].type;
          if (mtype->clear != NULL)
            mtype->clear (mtype, DSK_STRUCT_MEMBER_P (strct, s->members[member_index[j]].offset));
        }
    for (j = 0; j < s->n_members; j++)
      if (s->members[j].quantity == DSK_XML_BINDING_REPEATED
       || s->members[j].quantity == DSK_XML_BINDING_REQUIRED_REPEATED)
        {
          unsigned count = DSK_STRUCT_MEMBER (unsigned, strct, s->members[j].quantifier_offset);
          char *slab = DSK_STRUCT_MEMBER (char *, strct, s->members[j].offset);
          DskXmlBindingType *mtype = s->members[j].type;
          if (mtype->clear != NULL)
            for (k = 0; k < count; k++)
              mtype->clear (mtype, slab + mtype->sizeof_instance * k);
          dsk_free (slab);
        }
  }
  return DSK_FALSE;
}

dsk_boolean dsk_xml_binding_struct_parse (DskXmlBindingType *type,
		                          DskXml            *to_parse,
		                          void              *out,
		                          DskError         **error)
{
  if (!dsk_xml_is_element (to_parse, type->name))
    {
      if (to_parse->type == DSK_XML_ELEMENT)
        dsk_set_error (error, "expected element of type %s.%s, got <%s>",
                       type->ns->name, type->name, to_parse->str);
      else
        dsk_set_error (error, "expected element of type %s.%s, got text",
                       type->ns->name, type->name);
      return DSK_FALSE;
    }

  return parse_struct_ignore_outer_tag (type, to_parse, out, error);
}

static dsk_boolean
struct_members_to_xml        (DskXmlBindingType *type,
                              void              *strct,
                              unsigned          *n_nodes_out,
                              DskXml          ***nodes_out,
                              DskError         **error)
{
  DskXmlBindingTypeStruct *s = (DskXmlBindingTypeStruct *) type;
  DskXml **children;
  unsigned n_children = 0;
  unsigned i;
  for (i = 0; i < s->n_members; i++)
    switch (s->members[i].quantity)
      {
      case DSK_XML_BINDING_REQUIRED:
        n_children++;
        break;
      case DSK_XML_BINDING_OPTIONAL:
        if (DSK_STRUCT_MEMBER (dsk_boolean, strct, s->members[i].quantifier_offset))
          n_children++;
        break;
      case DSK_XML_BINDING_REPEATED:
      case DSK_XML_BINDING_REQUIRED_REPEATED:
        n_children += DSK_STRUCT_MEMBER (unsigned, strct, s->members[i].quantifier_offset);
        break;
      }
  *n_nodes_out = n_children;
  *nodes_out = dsk_malloc (sizeof (DskXml *) * n_children);
  children = *nodes_out;
  n_children = 0;
  DskXml *subxml;
  for (i = 0; i < s->n_members; i++)
    switch (s->members[i].quantity)
      {
      case DSK_XML_BINDING_REQUIRED:
        subxml = s->members[i].type->to_xml (s->members[i].type,
                                             DSK_STRUCT_MEMBER_P (strct, s->members[i].offset),
                                             error);
        if (subxml == NULL)
          goto error_cleanup;
        children[n_children++] = dsk_xml_new_take_1 (s->members[i].name, subxml);
        break;
      case DSK_XML_BINDING_OPTIONAL:
        if (DSK_STRUCT_MEMBER (dsk_boolean, strct, s->members[i].quantifier_offset))
          {
            subxml = s->members[i].type->to_xml (s->members[i].type,
                                                 DSK_STRUCT_MEMBER_P (strct, s->members[i].offset),
                                                 error);
            if (subxml == NULL)
              goto error_cleanup;
            children[n_children++] = dsk_xml_new_take_1 (s->members[i].name, subxml);
          }
        break;
      case DSK_XML_BINDING_REPEATED:
      case DSK_XML_BINDING_REQUIRED_REPEATED:
        {
          unsigned j, n;
          char *array = DSK_STRUCT_MEMBER (char *, strct, s->members[i].offset);
          n = DSK_STRUCT_MEMBER (unsigned, strct, s->members[i].quantifier_offset);
          for (j = 0; j < n; j++)
            {
              subxml = s->members[i].type->to_xml (s->members[i].type,
                                                   array,
                                                   error);
              children[n_children++] = dsk_xml_new_take_1 (s->members[i].name, subxml);
              array += s->members[i].type->sizeof_instance;
            }

            break;
          }
      }
  dsk_assert (n_children == *n_nodes_out);
  return DSK_TRUE;
error_cleanup:
  for (i = 0; i < n_children; i++)
    dsk_xml_unref (children[i]);
  dsk_free (children);
  return DSK_FALSE;
}
DskXml  *   dsk_xml_binding_struct_to_xml(DskXmlBindingType *type,
		                          const void        *data,
		                          DskError         **error)
{
  void *strct = * (void **) data;
  unsigned n_children;
  DskXml **children;
  if (!struct_members_to_xml (type, strct, &n_children, &children, error))
    {
      return NULL;
    }
  DskXml *rv = dsk_xml_new_take_n (type->name, n_children, children);
  dsk_free (children);
  return rv;

}

static void
clear_struct_members (DskXmlBindingType *type,
                      void              *strct)
{
  DskXmlBindingTypeStruct *s = (DskXmlBindingTypeStruct *) type;
  unsigned i;
  for (i = 0; i < s->n_members; i++)
    {
      DskXmlBindingType *mtype = s->members[i].type;
      void *mem = DSK_STRUCT_MEMBER_P (strct, s->members[i].offset);
      switch (s->members[i].quantity)
        {
        case DSK_XML_BINDING_REQUIRED:
          if (mtype->clear != NULL)
            mtype->clear (mtype, mem);
          break;
        case DSK_XML_BINDING_OPTIONAL:
          if (mtype->clear != NULL
           && DSK_STRUCT_MEMBER (dsk_boolean, strct, s->members[i].quantifier_offset))
            mtype->clear (mtype, mem);
          break;
        case DSK_XML_BINDING_REPEATED:
        case DSK_XML_BINDING_REQUIRED_REPEATED:
          {
            if (mtype->clear != NULL)
              {
                char *arr = * (char **) mem;
                unsigned j, n = DSK_STRUCT_MEMBER (unsigned, strct, s->members[i].quantifier_offset);
                for (j = 0; j < n; j++)
                  {
                    mtype->clear (mtype, arr);
                    arr += mtype->sizeof_instance;
                  }
              }
            dsk_free (* (char **) mem);
          }
          break;
        }
    }
}



void        dsk_xml_binding_struct_clear (DskXmlBindingType *type,
		                          void              *data)
{
  void *strct = * (void **) data;
  clear_struct_members (type, strct);
  dsk_free (strct);
}

DskXmlBindingTypeStruct *
dsk_xml_binding_struct_new (DskXmlBindingNamespace *ns,
                            const char        *struct_name,
                            unsigned           n_members,
                            const DskXmlBindingStructMember *members)
{
  DskXmlBindingTypeStruct *rv;
  unsigned tail_space = n_members * sizeof (DskXmlBindingStructMember);
  unsigned i;
  for (i = 0; i < n_members; i++)
    tail_space += strlen (members[i].name) + 1;
  tail_space += strlen (struct_name);
  rv = dsk_malloc (sizeof (DskXmlBindingTypeStruct) + tail_space);
  rv->base_type.is_fundamental = 0;
  rv->base_type.is_static = 0;
  rv->base_type.is_struct = 1;
  rv->base_type.is_union = 0;
  rv->base_type.sizeof_instance = sizeof (void*);
  rv->base_type.alignof_instance = sizeof (void*);
  rv->base_type.ns = ns;
  rv->base_type.parse = dsk_xml_binding_struct_parse;
  rv->base_type.to_xml = dsk_xml_binding_struct_to_xml;
  rv->base_type.clear = dsk_xml_binding_struct_clear;

  rv->n_members = n_members;
  rv->members = (DskXmlBindingStructMember*)(rv + 1);
  char *str_at;
  str_at = (char*)(rv->members + n_members);
  rv->base_type.name = str_at;
  str_at = stpcpy (str_at, struct_name) + 1;
  for (i = 0; i < n_members; i++)
    {
      rv->members[i].name = str_at;
      str_at = stpcpy (str_at, members[i].name) + 1;
    }
  return rv;
}

static dsk_boolean
is_empty_element (DskXml *xml)
{
  if (xml->type != DSK_XML_ELEMENT)
    return DSK_FALSE;
  if (xml->n_children == 0)
    return DSK_TRUE;
  if (xml->n_children > 1)
    return DSK_FALSE;
  return dsk_xml_is_whitespace (xml->children[0]);
}

/* unions */
dsk_boolean dsk_xml_binding_union_parse  (DskXmlBindingType *type,
		                          DskXml            *to_parse,
		                          void              *out,
		                          DskError         **error)
{
  DskXmlBindingTypeUnion *u = (DskXmlBindingTypeUnion *) type;
  int case_i;
  void *union_data = NULL;
  DskXmlBindingType *case_type;
  if (to_parse->type != DSK_XML_ELEMENT)
    {
      dsk_set_error (error, "cannot parse union from string");
      goto error_maybe_add_union_name;
    }
  case_i = dsk_xml_binding_type_union_lookup_case (u, to_parse->str);
  if (case_i < 0)
    {
      dsk_set_error (error, "bad case '%s' in union",
                     to_parse->str);
      goto error_maybe_add_union_name;
    }
  union_data = dsk_malloc (u->sizeof_union);
  case_type = u->cases[case_i].type;
  if (case_type == NULL)
    {
      if (!is_empty_element (to_parse))
        {
          dsk_set_error (error, "case %s has no data", to_parse->str);
          goto error_maybe_add_union_name;
        }
    }
  else if (u->cases[case_i].elide_struct_outer_tag)
    {
      dsk_assert (case_type->is_struct);
      if (!parse_struct_ignore_outer_tag (case_type, to_parse,
                                          ((char*) out) + u->variant_offset,
                                          error))
        goto error_maybe_add_union_name;
    }
  else
    {
      /* find solo child */
      DskXml *subxml = dsk_xml_find_solo_child (to_parse, error);
      if (subxml == NULL)
        goto error_maybe_add_union_name;

      if (!case_type->parse (case_type, subxml,
                             (char*) union_data + u->variant_offset,
                             error))
        {
          goto error_maybe_add_union_name;
        }
    }
  * (DskXmlBindingTypeUnionTag *) union_data = u->cases[case_i].tag;
  * (void **) out = union_data;
  return DSK_TRUE;

error_maybe_add_union_name:
  if (type->name != NULL)
    dsk_add_error_prefix (error, "in %s.%s", type->ns->name, type->name);
  dsk_free (union_data);
  return DSK_FALSE;
}

DskXml  *
dsk_xml_binding_union_to_xml (DskXmlBindingType *type,
                              const void        *data,
                              DskError         **error)
{
  DskXmlBindingTypeUnionTag tag;
  DskXmlBindingTypeUnion *u = (DskXmlBindingTypeUnion *) type;
  void *udata = * (void **) data;
  void *variant_data = (char *)udata + u->variant_offset;
  int case_i;
  tag = * (DskXmlBindingTypeUnionTag *) udata;
  case_i = dsk_xml_binding_type_union_lookup_case_by_tag (u, tag);
  if (case_i < 0)
    {
      dsk_set_error (error, "unknown tag %u in union, while converting to XML", tag);
      goto error_maybe_add_union_name;
    }
  if (u->cases[case_i].type == NULL)
    {
      return dsk_xml_new_empty (u->cases[case_i].name);
    }
  else if (u->cases[case_i].elide_struct_outer_tag)
    {
      DskXml **subnodes;
      unsigned n_subnodes;
      DskXml *rv;
      if (!struct_members_to_xml (u->cases[case_i].type,
                                  variant_data,
                                  &n_subnodes, &subnodes, error))
        goto error_maybe_add_union_name;
      rv = dsk_xml_new_take_n (u->cases[case_i].name, n_subnodes, subnodes);
      dsk_free (subnodes);
      return rv;
    }
  else
    {
      DskXmlBindingType *subtype = u->cases[case_i].type;
      DskXml *subxml = subtype->to_xml (subtype, variant_data, error);
      if (subxml == NULL)
        goto error_maybe_add_union_name;
      return dsk_xml_new_take_1 (u->cases[case_i].name, subxml);
    }

error_maybe_add_union_name:
  if (type->name)
    dsk_add_error_prefix (error, "in union %s.%s", type->ns->name, type->name);
  return DSK_FALSE;
}

void
dsk_xml_binding_union_clear  (DskXmlBindingType *type,
                              void              *out)
{
  DskXmlBindingTypeUnionTag tag;
  int case_i;
  DskXmlBindingTypeUnion *u = (DskXmlBindingTypeUnion *) type;
  void *udata = * (void **) out;
  void *variant_data = (char *)udata + u->variant_offset;
  DskXmlBindingType *case_type;
  tag = * (DskXmlBindingTypeUnionTag *) udata;
  case_i = dsk_xml_binding_type_union_lookup_case_by_tag (u, tag);
  dsk_assert (case_i >= 0);
  case_type = u->cases[case_i].type;
  if (case_type == NULL)
    {
      /* nothing to do */
    }
  else if (u->cases[case_i].elide_struct_outer_tag)
    clear_struct_members (case_type, variant_data);
  else if (case_type->clear != NULL)
    case_type->clear (case_type, variant_data);
  dsk_free (udata);
}
