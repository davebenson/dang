#include "dsk.h"

/* Stages.
   (1) lexing
        - create a sequence of tags, text (incl cdata), comments etc
   (2) namespace resolution (if we are interested in the output,
       or if we are still at a valid parse state)
   (3) navigating the parse-state tree
   (4) if we finish a node with at a state which returns, return the node.
*/

/* forward definitions for lexing (stage 1) */
typedef enum
{
  LEX_DEFAULT,                  /* the usual state parsing text */
  LEX_DEFAULT_CHAR_ENTITY,      /* the usual state parsing text */
  LEX_LT,                       /* just got an "<" */
  LEX_OPEN_ELEMENT_NAME,        /* in element-name of open tag */
  LEX_OPEN_IN_ATTRS,            /* waiting for attribute-name or ">" */
  LEX_OPEN_IN_ATTR_NAME,        /* in attribute name */
  LEX_OPEN_AFTER_ATTR_NAME,     /* after attribute name */
  LEX_OPEN_AFTER_ATTR_NAME_EQ,  /* after attribute name= */
  LEX_OPEN_IN_ATTR_VALUE_SQ     /* in single-quoted attribute name */
  LEX_OPEN_IN_ATTR_VALUE_DQ     /* in double-quoted attribute name */
  LEX_OPEN_IN_ATTR_VALUE_SQ_CHAR_ENTITY, /* in attribute-value, having      */
  LEX_OPEN_IN_ATTR_VALUE_DQ_CHAR_ENTITY, /* ...an "&" waiting for semicolon */
  LEX_LT_SLASH,                 /* just got an "</" */
  LEX_CLOSE_ELEMENT_NAME,       /* in element-name of open tag */
} LexState;




/* --- configuration --- */
typedef struct _ParseStateTransition ParseStateTransition;
struct _ParseStateTransition
{
  char *str;
  ParseState *state;
};
typedef struct _ParseState ParseState;
struct _ParseState
{
  unsigned n_ret;
  unsigned *ret_indices;

  unsigned *n_transitions;
  ParseStateTransitions *transitions;

  ParseState *wildcard_transition;
};

struct _DskXmlParserConfig
{
  /* sorted by url */
  unsigned n_ns;
  DskXmlParserNamespaceConfig *ns;

  ParseState base;

  unsigned ignore_ns : 1;
  unsigned suppress_whitespace : 1;
  unsigned suppress_comments : 1;
  unsigned destroyed : 1;
};

/* --- parser --- */
typedef struct _NsAbbrevMap NsAbbrevMap;
struct _NsAbbrevMap
{
  char *abbrev;         /* used in source doc */
  DskXmlParserNamespaceConfig *translate;

  /* list of namespace abbreviations defined at a single element */
  NsAbbrevMap *defined_list_next;

  /* rbtree by name */
  NsAbbrevMap *parent, *left, *right;
  unsigned is_red : 1;

  /* are the source-doc abbreviation and the config abbreviation equal? */
  unsigned is_nop : 1;
};

struct _StackNode
{
  char *name;
  char **kv;
  ParseState *state;
  NsAbbrevMap *defined_list;
  unsigned n_children;
  DskXml **children;
};

struct _DskXmlParser
{
  DskXmlFilename *filename;
  unsigned line_no;
  DskBuffer buffer;

  NsAbbrevMap *ns_map;

  unsigned stack_size;
  StackNode stack[MAX_DEPTH];

  DskXmlParserNamespaceConfig *config;
};


DskXmlParserConfig *
dsk_xml_parser_config_new (DskXmlParserFlags flags,
			   unsigned          n_ns,
			   const DskXmlParserNamespaceConfig *ns,
			   unsigned          n_xpaths,
			   char             *xpaths)
{
  /* copy and sort the namespace mapping, if enabled */
  DskXmlParserNamespaceConfig *ns_slab = NULL;
  if ((ns->flags & DSK_XML_PARSER_IGNORE_NS) == 0)
    {
      unsigned total_strlen = 0;
      unsigned i;
      char *at;
      for (i = 0; i < n_ns; i++)
        total_strlen += strlen (ns[i].url) + 1
                      + strlen (ns[i].prefix) + 1;
      ns_slab = dsk_malloc (sizeof (DskXmlParserNamespaceConfig) * n_ns
                            + total_strlen);
      at = (char*)(ns_slab + n_ns);
      for (i = 0; i < n_ns; i++)
        {
          ns_slab[i].url = at;
          at = dsk_stpcpy (at, ns[i].url);
          ns_slab[i].prefix = at;
          at = dsk_stpcpy (at, ns[i].prefix);
        }
    }

  ...
}

void
dsk_xml_parser_config_destroy (DskXmlParserConfig *config)
{
  dsk_assert (!config->destroyed);
  config->destroyed = 1;
  dsk_xml_parser_config_unref (config);
}

DskXmlParser *
dsk_xml_parser_new (DskXmlParserConfig *config,
                    const char         *display_filename)
{
  DskXmlParser *parser;
  dsk_assert (config != NULL);
  parser = dsk_malloc (sizeof (DskXmlParser));

  parser->filename = display_filename ? new_filename (display_filename) : NULL;
  parser->line_no = 1;
  dsk_buffer_init (&parser->buffer);
  parser->ns_map = NULL;
  parser->stack_size = 0;
  parser->config = config;
  config->ref_count += 1;
  return parser;
}


DskXml *
dsk_xml_parser_pop (DskXmlParser       *parser,
                    unsigned           *xpath_index_out)
{
  ...
}

dsk_boolean
dsk_xml_parser_feed(DskXmlParser       *parser,
                    size_t              len,
                    const char         *data,
                    DskError          **error_out)

{
  ...
}

void
dsk_xml_parser_free(DskXmlParser       *parser)
{
  ...
}


