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
    for (i = 0; i < child->n_children; i++)
      if (!xml_is_whitespace (child->children[i]))
        return child->children[i];
  return NULL;  /* can't happen since is_value_bearing_node() returned TRUE */
}

/* structures */
dsk_boolean dsk_xml_binding_struct_parse (DskXmlBindingType *type,
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
                ...
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
                                        slab + member->type->sizeof_struct * idx,
                                        error))
                {
                  ...
                  goto error_cleanup;
                }
              *p_idx += 1;
            }
            break;
          }
      }
  * (void **) out = strct;
  return DSK_TRUE;

cleanup:
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
      if (s->members[j] == DSK_XML_BINDING_REPEATED
       || s->members[j] == DSK_XML_BINDING_REQUIRED_REPEATED)
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

DskXml  *   dsk_xml_binding_struct_to_xml(DskXmlBindingType *type,
		                          const char        *data,
		                          DskError         **error)
{
  ...
}

void        dsk_xml_binding_struct_clear (DskXmlBindingType *type,
		                          void              *out)
{
  DskXmlBindingTypeStruct *s = (DskXmlBindingTypeStruct *) type;
  unsigned i;
  for (i = 0; i < s->n_members; i++)
    switch (s->members[i].quantity)
      {
        ...
      }
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
  rv = dsk_malloc (sizeof (DskXmlBindingTypeStruct) + tail_space);
  rv->base.is_fundamental = 0;
  rv->base.is_static = 0;
  rv->base.is_struct = 1;
  rv->base.is_union = 0;
  rv->base.sizeof_type = ???;
  rv->base.alignof_type = ???;
  rv->base.ns = ???;
  rv->base.name = ???;
  rv->base.parse = dsk_xml_binding_struct_parse;
  rv->base.to_xml = dsk_xml_binding_struct_to_xml;
  rv->base.clear = dsk_xml_binding_struct_clear;

  rv->n_members = n_members;
  rv->members = (DskXmlBindingStructMember*)(rv + 1);
  str_at = (char*)(rv->members + n_members);
  for (i = 0; i < n_members; i++)
    {
      ...
    }
}

/* unions */
dsk_boolean dsk_xml_binding_union_parse  (DskXmlBindingType *type,
		                          DskXml            *to_parse,
		                          void              *out,
		                          DskError         **error)
{
  ...
}

DskXml  *   dsk_xml_binding_union_to_xml (DskXmlBindingType *type,
		                          const char        *data,
		                          DskError         **error)
{
  ...
}

void        dsk_xml_binding_union_clear  (DskXmlBindingType *type,
		                          void              *out)
{
  ...
}

