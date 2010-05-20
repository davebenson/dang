#include <string.h>
#include <stdlib.h>
#include "dsk.h"

#define MAX_DEPTH       64

/* References:
        The Annotated XML Specification, by Tim Bray
        http://www.xml.com/axml/testaxml.htm
 */
/* Layers
   (1) lexing
        - create a sequence of tags, text (incl cdata), comments etc
   (2) parsing occur in phases:
       (a) character set validation
       (b) namespace resolution
       (c) parse-state tree navigation, and returning nodes
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
  LEX_OPEN_IN_ATTR_VALUE_SQ,    /* in single-quoted attribute name */
  LEX_OPEN_IN_ATTR_VALUE_DQ,    /* in double-quoted attribute name */
  LEX_OPEN_IN_ATTR_VALUE_SQ_CHAR_ENTITY, /* in attribute-value, having      */
  LEX_OPEN_IN_ATTR_VALUE_DQ_CHAR_ENTITY, /* ...an "&" waiting for semicolon */
  LEX_LT_SLASH,                 /* just got an "</" */
  LEX_CLOSE_ELEMENT_NAME,       /* in element-name of open tag */
  LEX_LT_BANG,
  LEX_LT_BANG_MINUS,            /* <!- */
  LEX_COMMENT,                  /* <!-- */
  LEX_COMMENT_MINUS,            /* <!-- comment - */
  LEX_COMMENT_MINUS_MINUS,      /* <!-- comment -- */
  LEX_LT_BANG_LBRACK,
  LEX_LT_BANG_LBRACK_IN_CDATAHDR,  /* <![CD   (for example, nchar of "CDATA" given by ??? */
  LEX_LT_BANG_LBRACK_CDATAHDR,  /* <![CD   (for example, nchar of "CDATA" given by ??? */
  LEX_CDATA,
  LEX_CDATA_RBRACK,
  LEX_CDATA_RBRACK_RBRACK,
} LexState;

#define WHITESPACE_CASES  case ' ': case '\t': case '\r': case '\n'

/* --- configuration --- */
typedef struct _ParseStateTransition ParseStateTransition;
typedef struct _ParseState ParseState;
struct _ParseStateTransition
{
  char *str;
  ParseState *state;
};
struct _ParseState
{
  unsigned n_ret;
  unsigned *ret_indices;

  unsigned *n_transitions;
  ParseStateTransition *transitions;

  ParseState *wildcard_transition;
};

struct _DskXmlParserConfig
{
  /* sorted by url */
  unsigned n_ns;
  DskXmlParserNamespaceConfig *ns;

  ParseState base;

  unsigned ref_count;

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

typedef struct _StackNode StackNode;
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

  /* for text, comments, etc */
  DskBuffer buffer;

  NsAbbrevMap *ns_map;

  unsigned stack_size;
  StackNode stack[MAX_DEPTH];

  unsigned n_to_be_returned;

  LexState lex_state;

  DskXmlParserNamespaceConfig *config;
};

/* utility: split our xml-path into components, checking for unallowed things. */
static char **
validate_and_split_xpath (const char *xmlpath,
                          DskError  **error)
{
  dsk_boolean slash_allowed = DSK_FALSE;
  unsigned n_slashes = 0;
  const char *at;
  char **rv;
  unsigned i;
  for (at = xmlpath; *at; at++)
    switch (*at)
      {
      WHITESPACE_CASES:
        dsk_set_error (error, "whitespace not allowed in xmlpath");
        return NULL;
      case '/':
        if (!slash_allowed)
          {
            if (at == xmlpath)
              dsk_set_error (error, "initial '/' in xmlpath not allowed");
            else
              dsk_set_error (error, "two consecutive '/'s in xmlpath not allowed");
            return DSK_FALSE;
          }
        n_slashes++;
        slash_allowed = DSK_FALSE;
        break;
      default:
        slash_allowed = DSK_TRUE;
      }
  if (!slash_allowed && at > xmlpath)
    {
      dsk_set_error (error, "final '/' in xmlpath not allowed");
      return DSK_FALSE;
    }
  if (!slash_allowed)
    {
      rv = dsk_malloc0 (sizeof (char *));
      return rv;
    }
  rv = dsk_malloc (sizeof (char*) * (n_slashes + 2));
  i = 0;
  for (at = xmlpath; at != NULL; )
    {
      const char *slash = strchr (at, '/');
      if (slash)
        {
          rv[i++] = dsk_strdup_slice (at, slash);
          at = slash + 1;
        }
      else
        {
          rv[i++] = dsk_strdup (at);
          at = NULL;
        }
    }
  rv[i] = NULL;
  return rv;
}




dsk_boolean
dsk_xml_parser_feed(DskXmlParser       *parser,
                          unsigned            len,
                          char               *data,
                                  DskError          **error_out)
{
  dsk_boolean suppress;
#define MAYBE_RETURN            do{if(len == 0) return DSK_TRUE;}while(0)
#define ADVANCE_NOT_NL          do{len--; data++;}while(0)
#define ADVANCE_MAYBE_NL          do{if (*data == '\n')lineno++; len--; data++;}while(0)
#define ADVANCE_MAYBE_NL          do{if (*data == '\n')lineno++; len--; data++;}while(0)
  suppress = parser->n_to_be_returned == 0
         && (parser->stack_size == 0 || parser->stack[parser->stack_size-1].state == NULL);
  switch (parser->lex_state)
    {
    case LEX_DEFAULT:
    label__LEX_DEFAULT:
      ...
    case LEX_DEFAULT_CHAR_ENTITY:
    label__LEX_DEFAULT_CHAR_ENTITY:
      if (suppress)
        {
          const char *semicolon = memchr (data, ';', len);
          if (semicolon == NULL)
            return DSK_TRUE;
          len -= (semicolon + 1 - data);
          data = semicolon + 1;
          parser->lex_state = LEX_DEFAULT;
          goto label__LEX_DEFAULT;;
        }
      else
        {
          ...
        }
    case LEX_LT:
    label__LEX_LT:
      switch (*buf)
        {
        case '!': CONSUME_CHAR_AND_SWITCH_STATE (LEX_LT_BANG);
        case '/': CONSUME_CHAR_AND_SWITCH_STATE (LEX_LT_SLASH);
        case '?': CONSUME_CHAR_AND_SWITCH_STATE (LEX_PROCESSING_INSTRUCTION);
        WHITESPACE_CASES: 
          ADVANCE;
          MAYBE_RETURN;
          goto label__LEX_LT;
        default:
          dsk_buffer_append_byte (&parser->buffer, *buf);
          CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_ELEMENT_NAME);
        }

    case LEX_OPEN_ELEMENT_NAME:
    label__LEX_OPEN_ELEMENT_NAME:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            dsk_buffer_append_byte (&parser->buffer, 0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTRS);
          case '>':
            dsk_buffer_append_byte (&parser->buffer, 0);
            handle_open_element (parser);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_DEFAULT);
          case '/':
            ...
          case '=':
            goto disallowed_char;
          default:
            dsk_buffer_append_byte (&parser->buffer, *buf);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_ELEMENT_NAME);
          }
      }
    case LEX_OPEN_IN_ATTRS:
    label__LEX_OPEN_IN_ATTRS:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTRS);
          case '>':
            handle_open_element (parser);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_DEFAULT);
          case '/':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_CLOSE);
          case '=':
            goto disallowed_char;
          default:
            dsk_buffer_append_byte (&parser->buffer, *buf);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_NAME);
          }
      }
    case LEX_OPEN_IN_ATTR_NAME:
    label__LEX_OPEN_IN_ATTR_NAME:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            dsk_buffer_append_byte (&parser->buffer, 0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_AFTER_ATTR_NAME);
          case '>': case '/':
            goto disallowed_char;
          case '=':
            dsk_buffer_append_byte (&parser->buffer, 0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_AFTER_ATTR_NAME_EQ);
          default:
            dsk_buffer_append_byte (&parser->buffer, *buf);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_NAME);
          }
      }
    case LEX_OPEN_AFTER_ATTR_NAME:
    label__LEX_OPEN_AFTER_ATTR_NAME:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            ADVANCE;
            MAYBE_RETURN;
            goto label__LEX_OPEN_AFTER_ATTR_NAME;
          case '>': case '/':
            goto disallowed_char;
          case '=':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_AFTER_ATTR_NAME_EQ);
          default:
            goto disallowed_char;
          }
      }
    case LEX_OPEN_AFTER_ATTR_NAME_EQ:
    label__LEX_OPEN_AFTER_ATTR_NAME_EQ:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            ADVANCE;
            MAYBE_RETURN;
            goto label__LEX_OPEN_AFTER_ATTR_NAME;
          case '"':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_VALUE_DQ);
          case '\'':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_VALUE_SQ);
          default:
            goto disallowed_char;
          }
      }
    case LEX_OPEN_IN_ATTR_VALUE_SQ:
    label__LEX_OPEN_IN_ATTR_VALUE_SQ:
      {
        return DSK_TRUE;
      }
    case LEX_DEFAULT_CHAR_ENTITY:
    label__LEX_DEFAULT_CHAR_ENTITY:
      /* in 'skip' mode, looking for a semicolon suffices */
      {
        const char *semicolon = memchr (buf, ';', len);
        if (semicolon == NULL)
          return DSK_TRUE;
        len -= (semicolon + 1 - buf);
        buf = semicolon + 1;
        parser->lex_state = LEX_DEFAULT;
        goto skip__default;
      }
    case LEX_LT:
    label__LEX_LT:
      switch (*buf)
        {
        case '!': CONSUME_CHAR_AND_SWITCH_STATE (LEX_LT_BANG);
        case '/': CONSUME_CHAR_AND_SWITCH_STATE (LEX_LT_SLASH);
        case '?': CONSUME_CHAR_AND_SWITCH_STATE (LEX_PROCESSING_INSTRUCTION);
        WHITESPACE_CASES: 
          ADVANCE;
          MAYBE_RETURN;
          goto label__LEX_LT;
        default:
          dsk_buffer_append_byte (&parser->buffer, *buf);
          CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_ELEMENT_NAME);
        }

    case LEX_OPEN_ELEMENT_NAME:
    label__LEX_OPEN_ELEMENT_NAME:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            dsk_buffer_append_byte (&parser->buffer, 0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTRS);
          case '>':
            dsk_buffer_append_byte (&parser->buffer, 0);
            handle_open_element (parser);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_DEFAULT);
          case '/':
            ...
          case '=':
            goto disallowed_char;
          default:
            dsk_buffer_append_byte (&parser->buffer, *buf);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_ELEMENT_NAME);
          }
      }
    case LEX_OPEN_IN_ATTRS:
    label__LEX_OPEN_IN_ATTRS:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTRS);
          case '>':
            handle_open_element (parser);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_DEFAULT);
          case '/':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_CLOSE);
          case '=':
            goto disallowed_char;
          default:
            dsk_buffer_append_byte (&parser->buffer, *buf);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_NAME);
          }
      }
    case LEX_OPEN_IN_ATTR_NAME:
    label__LEX_OPEN_IN_ATTR_NAME:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            dsk_buffer_append_byte (&parser->buffer, 0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_AFTER_ATTR_NAME);
          case '>': case '/':
            goto disallowed_char;
          case '=':
            dsk_buffer_append_byte (&parser->buffer, 0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_AFTER_ATTR_NAME_EQ);
          default:
            dsk_buffer_append_byte (&parser->buffer, *buf);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_NAME);
          }
      }
    case LEX_OPEN_AFTER_ATTR_NAME:
    label__LEX_OPEN_AFTER_ATTR_NAME:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            ADVANCE;
            MAYBE_RETURN;
            goto label__LEX_OPEN_AFTER_ATTR_NAME;
          case '>': case '/':
            goto disallowed_char;
          case '=':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_AFTER_ATTR_NAME_EQ);
          default:
            goto disallowed_char;
          }
      }
    case LEX_OPEN_AFTER_ATTR_NAME_EQ:
    label__LEX_OPEN_AFTER_ATTR_NAME_EQ:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            ADVANCE;
            MAYBE_RETURN;
            goto label__LEX_OPEN_AFTER_ATTR_NAME;
          case '"':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_VALUE_DQ);
          case '\'':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_VALUE_SQ);
          default:
            goto disallowed_char;
          }
      }
    case LEX_OPEN_IN_ATTR_VALUE_SQ:
    label__LEX_OPEN_IN_ATTR_VALUE_SQ:
      {
        switch (*data)
          {
          case '\'':
            dsk_buffer_append_byte (&parser->buffer, 0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTRS);
          case '&':
            parser->entity_buf_len = 0;
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_VALUE_SQ_CHAR_ENTITY);
          default:
            dsk_buffer_append_byte (&parser->buffer, *buf);
            ADVANCE_MAYBE_NEWLINE;
            MAYBE_RETURN;
            goto label__LEX_OPEN_IN_ATTR_VALUE_SQ;
          }
      }
    case LEX_OPEN_IN_ATTR_VALUE_DQ:
    label__LEX_OPEN_IN_ATTR_VALUE_DQ:
      {
        switch (*data)
          {
          case '"':
            dsk_buffer_append_byte (&parser->buffer, 0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTRS);
          case '&':
            parser->entity_buf_len = 0;
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_VALUE_DQ_CHAR_ENTITY);
          default:
            dsk_buffer_append_byte (&parser->buffer, *buf);
            ADVANCE_MAYBE_NEWLINE;
            MAYBE_RETURN;
            goto label__LEX_OPEN_IN_ATTR_VALUE_DQ;
          }
      }
    case LEX_OPEN_IN_ATTR_VALUE_SQ_CHAR_ENTITY:
    label__LEX_OPEN_IN_ATTR_VALUE_SQ_CHAR_ENTITY:
      ...
    case LEX_OPEN_IN_ATTR_VALUE_DQ_CHAR_ENTITY:
    label__LEX_OPEN_IN_ATTR_VALUE_DQ_CHAR_ENTITY:
      ...
    case LEX_LT_SLASH:
      ...
    case LEX_CLOSE_ELEMENT_NAME:
      ...
    case LEX_LT_BANG:
      ...
    case LEX_LT_BANG_MINUS:
      ...
    case LEX_COMMENT:
      ...
    case LEX_COMMENT_MINUS:
      ...
    case LEX_COMMENT_MINUS_MINUS:
      ...
    case LEX_LT_BANG_LBRACK:
      ...
    case LEX_LT_BANG_LBRACK_IN_CDATAHDR:
      ...
    case LEX_LT_BANG_LBRACK_CDATAHDR:
      ...
    case LEX_CDATA:
      ...
    case LEX_CDATA_RBRACK:
      ...
    case LEX_CDATA_RBRACK_RBRACK:
      ...
    }

#undef MAYBE_RETURN
#undef CONSUME_CHAR_AND_SWITCH_STATE
#undef ADVANCE
}

void
dsk_xml_parser_config_unref (DskXmlParserConfig *config)
{
  if (--(config->ref_count) == 0)
    {
      dsk_assert (config->destroyed);
      dsk_free (config->ns);
      destruct_parse_state_recursive (&config->base);
      dsk_free (config);
    }
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

void
dsk_xml_parser_free(DskXmlParser       *parser)
{
  ...
}


