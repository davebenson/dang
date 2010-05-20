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
  LEX_CLOSE_ELEMENT_NAME,       /* in element-name of close tag */
  LEX_AFTER_CLOSE_ELEMENT_NAME,       /* after element-name of close tag */
  LEX_OPEN_CLOSE,               /* got a slash after an open tag */
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
  LEX_PROCESSING_INSTRUCTION,
  LEX_PROCESSING_INSTRUCTION_QM,        /* after question mark in PI */
  LEX_BANG_DIRECTIVE     /* this encompasses ELEMENT, ATTLIST, ENTITY, DOCTYPE declarations */
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
  unsigned include_comments : 1;
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

#define MAX_CHAR_ENTITY_LENGTH  16

struct _DskXmlParser
{
  DskXmlFilename *filename;
  unsigned line_no;

  /* for text, comments, etc */
  DskBuffer buffer;

  NsAbbrevMap *ns_map;

  unsigned stack_size;
  StackNode stack[MAX_DEPTH];

  char entity_buf[MAX_CHAR_ENTITY_LENGTH];
  unsigned entity_buf_len;

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

  config = ...;
}

/* --- character entities --- */

static dsk_boolean
handle_char_entity (DskXmlParser *parser,
                    DskError    **error)
{
  const char *b = parser->entity_buf;
  switch (parser->entity_buf_len)
    {
    case 0: case 1:
      dsk_set_error (error, "character entity too short (%u bytes)", parser->entity_buf_len);
      return DSK_FALSE;
    case 2:
      switch (b[0])
        {
        case 'l': case 'L':
          if (b[1] == 't' || b[1] == 'T')
            {
              dsk_buffer_append_byte (&parser->buffer, '<');
              return DSK_TRUE;
            }
          break;
        case 'g': case 'G':
          if (b[1] == 't' || b[1] == 'T')
            {
              dsk_buffer_append_byte (&parser->buffer, '>');
              return DSK_TRUE;
            }
          break;
        }

    case 3:
      switch (parser->entity_buf[0])
        {
        case 'a': case 'A':
          if ((b[1] == 'm' || b[1] == 'M')
           || (b[2] == 'p' || b[2] == 'P'))
            {
              dsk_buffer_append_byte (&parser->buffer, '&');
              return DSK_TRUE;
            }
          break;
        }
    case 4:
      switch (parser->entity_buf[0])
        {
        case 'a': case 'A':
          if ((b[1] == 'p' || b[1] == 'P')
           || (b[2] == 'o' || b[2] == 'O')
           || (b[3] == 's' || b[3] == 'S'))
            {
              dsk_buffer_append_byte (&parser->buffer, '\'');
              return DSK_TRUE;
            }
          break;
        case 'q': case 'Q':
          if ((b[1] == 'u' || b[1] == 'U')
           || (b[2] == 'o' || b[2] == 'O')
           || (b[3] == 't' || b[3] == 'T'))
            {
              dsk_buffer_append_byte (&parser->buffer, '"');
              return DSK_TRUE;
            }
          break;
        }
      break;
    default:
      dsk_set_error (error, "character entity too long (%u bytes)", parser->entity_buf_len);
      return DSK_FALSE;
    }
  dsk_set_error (error, "unknown character entity (&%.*s;)", parser->entity_buf_len, parser->entity_buf);
  return DSK_FALSE;
}

/* --- handling open/close tags --- */

static dsk_boolean handle_open_element (DskXmlParser *parser,
                                        DskError    **error);
static dsk_boolean handle_close_element (DskXmlParser *parser,
                                        DskError    **error);
static dsk_boolean handle_open_close_element (DskXmlParser *parser,
                                              DskError    **error);
/* --- lexing --- */
dsk_boolean
dsk_xml_parser_feed(DskXmlParser       *parser,
                    unsigned            len,
                    const char               *data,
                    DskError          **error)
{
  dsk_boolean suppress;
#define BUFFER_CLEAR            dsk_buffer_clear (&parser->buffer)
#define APPEND_BYTE(val)        dsk_buffer_append_byte (&parser->buffer, (val))
#define MAYBE_RETURN            do{if(len == 0) return DSK_TRUE;}while(0)
#define ADVANCE_NON_NL          do{len--; data++;}while(0)
#define ADVANCE_CHAR            do{if (*data == '\n')parser->line_no++; len--; data++;}while(0)
#define ADVANCE_NL              do{parser->line_no++; len--; data++;}while(0)
#define CONSUME_CHAR_AND_SWITCH_STATE(STATE) do{ parser->lex_state = STATE; ADVANCE_CHAR; MAYBE_RETURN; goto label__##STATE; }while(0)
#define CONSUME_NL_AND_SWITCH_STATE(STATE) do{ parser->lex_state = STATE; ADVANCE_NL; MAYBE_RETURN; goto label__##STATE; }while(0)
#define CONSUME_NON_NL_AND_SWITCH_STATE(STATE) do{ parser->lex_state = STATE; ADVANCE_NON_NL; MAYBE_RETURN; goto label__##STATE; }while(0)
#define CUT_TO_BUFFER           do {if (!suppress && start < data) dsk_buffer_append (&parser->buffer, data-start, start); }while(0)
  suppress = parser->n_to_be_returned == 0
         && (parser->stack_size == 0 || parser->stack[parser->stack_size-1].state == NULL);
  switch (parser->lex_state)
    {
    case LEX_DEFAULT:
    label__LEX_DEFAULT:
      {
        const char *start = data;
        switch (*data)
          {
          case '<':
            CUT_TO_BUFFER;
            CONSUME_NON_NL_AND_SWITCH_STATE (LEX_LT);
          case '&':
            CUT_TO_BUFFER;
            parser->entity_buf_len = 0;
            CONSUME_NON_NL_AND_SWITCH_STATE (LEX_DEFAULT_CHAR_ENTITY);
          default:
            ADVANCE_CHAR;
            if (len == 0)
              {
                if (!suppress)
                  CUT_TO_BUFFER;
                return DSK_TRUE;
              }
          }
      }

    case LEX_DEFAULT_CHAR_ENTITY:
    label__LEX_DEFAULT_CHAR_ENTITY:
      {
        const char *semicolon = memchr (data, ';', len);
        unsigned new_elen;
        if (semicolon == NULL)
          new_elen = parser->entity_buf_len + len;
        else
          new_elen = parser->entity_buf_len + (semicolon - data);
        if (new_elen > MAX_CHAR_ENTITY_LENGTH)
          goto char_entity_too_long;
        if (suppress)
          {
            if (semicolon == NULL)
              {
                parser->entity_buf_len = new_elen;
                return DSK_TRUE;
              }
          }
        else
          {
            if (semicolon == NULL)
              {
                memcpy (parser->entity_buf + parser->entity_buf_len, data, len);
                parser->entity_buf_len += len;
                return DSK_TRUE;
              }
            memcpy (parser->entity_buf + parser->entity_buf_len, data, semicolon - data);
            if (!handle_char_entity (parser, error))
              return DSK_FALSE;
          }
        len -= (semicolon + 1 - data);
        data = semicolon + 1;
        parser->lex_state = LEX_DEFAULT;
        MAYBE_RETURN;
        goto label__LEX_DEFAULT;
      }
    case LEX_LT:
    label__LEX_LT:
      switch (*data)
        {
        case '!': CONSUME_CHAR_AND_SWITCH_STATE (LEX_LT_BANG);
        case '/': CONSUME_CHAR_AND_SWITCH_STATE (LEX_LT_SLASH);
        case '?': CONSUME_CHAR_AND_SWITCH_STATE (LEX_PROCESSING_INSTRUCTION);
        WHITESPACE_CASES: 
          ADVANCE_CHAR;
          MAYBE_RETURN;
          goto label__LEX_LT;
        default:
          APPEND_BYTE (*data);
          CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_ELEMENT_NAME);
        }

    case LEX_OPEN_ELEMENT_NAME:
    label__LEX_OPEN_ELEMENT_NAME:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            APPEND_BYTE (0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTRS);
          case '>':
            APPEND_BYTE (0);
            if (!handle_open_element (parser, error))
              return DSK_FALSE;
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_DEFAULT);
          case '/':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_CLOSE);
          case '=':
            goto disallowed_char;
          default:
            APPEND_BYTE (*data);
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
            if (!handle_open_element (parser, error))
              return DSK_FALSE;
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_DEFAULT);
          case '/':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_CLOSE);
          case '=':
            goto disallowed_char;
          default:
            APPEND_BYTE (*data);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_NAME);
          }
      }
    case LEX_OPEN_IN_ATTR_NAME:
    label__LEX_OPEN_IN_ATTR_NAME:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            APPEND_BYTE (0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_AFTER_ATTR_NAME);
          case '>': case '/':
            goto disallowed_char;
          case '=':
            APPEND_BYTE (0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_AFTER_ATTR_NAME_EQ);
          default:
            APPEND_BYTE (*data);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_NAME);
          }
      }
    case LEX_OPEN_AFTER_ATTR_NAME:
    label__LEX_OPEN_AFTER_ATTR_NAME:
      {
        switch (*data)
          {
          WHITESPACE_CASES:
            ADVANCE_CHAR;
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
            ADVANCE_CHAR;
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
            APPEND_BYTE (0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTRS);
          case '&':
            parser->entity_buf_len = 0;
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_VALUE_SQ_CHAR_ENTITY);
          default:
            APPEND_BYTE (*data);
            ADVANCE_CHAR;
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
            APPEND_BYTE (0);
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTRS);
          case '&':
            parser->entity_buf_len = 0;
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_VALUE_DQ_CHAR_ENTITY);
          default:
            APPEND_BYTE (*data);
            ADVANCE_CHAR;
            MAYBE_RETURN;
            goto label__LEX_OPEN_IN_ATTR_VALUE_DQ;
          }
      }
    case LEX_OPEN_IN_ATTR_VALUE_SQ_CHAR_ENTITY:
    case LEX_OPEN_IN_ATTR_VALUE_DQ_CHAR_ENTITY:
    label__LEX_OPEN_IN_ATTR_VALUE_SQ_CHAR_ENTITY:
    label__LEX_OPEN_IN_ATTR_VALUE_DQ_CHAR_ENTITY:
      {
        const char *semicolon = memchr (data, ';', len);
        unsigned new_elen;
        if (semicolon == NULL)
          new_elen = parser->entity_buf_len + len;
        else
          new_elen = parser->entity_buf_len + (semicolon - data);
        if (new_elen > MAX_CHAR_ENTITY_LENGTH)
          goto char_entity_too_long;
        if (suppress)
          {
            if (semicolon == NULL)
              {
                parser->entity_buf_len = new_elen;
                return DSK_TRUE;
              }
          }
        else
          {
            if (semicolon == NULL)
              {
                memcpy (parser->entity_buf + parser->entity_buf_len, data, len);
                parser->entity_buf_len += len;
                return DSK_TRUE;
              }
            memcpy (parser->entity_buf + parser->entity_buf_len, data, semicolon - data);
            if (!handle_char_entity (parser, error))
              return DSK_FALSE;
          }
        len -= (semicolon + 1 - data);
        data = semicolon + 1;
        if (parser->lex_state == LEX_OPEN_IN_ATTR_VALUE_SQ_CHAR_ENTITY)
          {
            parser->lex_state = LEX_OPEN_IN_ATTR_VALUE_SQ;
            MAYBE_RETURN;
            goto label__LEX_OPEN_IN_ATTR_VALUE_SQ;
          }
        else
          {
            parser->lex_state = LEX_OPEN_IN_ATTR_VALUE_DQ;
            MAYBE_RETURN;
            goto label__LEX_OPEN_IN_ATTR_VALUE_DQ;
          }
      }
    case LEX_LT_SLASH:
    label__LEX_LT_SLASH:
      switch (*data)
        {
        WHITESPACE_CASES:
          ADVANCE_CHAR;
          MAYBE_RETURN;
          goto label__LEX_LT_SLASH;
        case '<': case '>': case '=':
          goto disallowed_char;
        default:
          APPEND_BYTE (*data);
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_CLOSE_ELEMENT_NAME);
        }

    case LEX_CLOSE_ELEMENT_NAME:
    label__LEX_CLOSE_ELEMENT_NAME:
      switch (*data)
        {
        WHITESPACE_CASES:
          CONSUME_CHAR_AND_SWITCH_STATE (LEX_AFTER_CLOSE_ELEMENT_NAME);
        case '<': case '=':
          goto disallowed_char;
        case '>':
          if (!handle_close_element (parser, error))
            return DSK_FALSE;
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_DEFAULT);
        default:
          APPEND_BYTE (*data);
          ADVANCE_NON_NL;
          MAYBE_RETURN;
          goto label__LEX_CLOSE_ELEMENT_NAME;
        }
    case LEX_AFTER_CLOSE_ELEMENT_NAME:
    label__LEX_AFTER_CLOSE_ELEMENT_NAME:
      switch (*data)
        {
        WHITESPACE_CASES:
          ADVANCE_CHAR;
          MAYBE_RETURN;
          goto label__LEX_AFTER_CLOSE_ELEMENT_NAME;
        case '>':
          if (!handle_close_element (parser, error))
            return DSK_FALSE;
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_DEFAULT);
        default:
          goto disallowed_char;
        }
    case LEX_OPEN_CLOSE:
    label__LEX_OPEN_CLOSE:
      switch (*data)
        {
        WHITESPACE_CASES:
          ADVANCE_CHAR;
          MAYBE_RETURN;
          goto label__LEX_OPEN_CLOSE;
        case '>':
          if (!suppress)
            {
              if (!handle_open_close_element (parser, error))
                return DSK_FALSE;
            }
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_DEFAULT);
        default:
          goto disallowed_char;
        }
    case LEX_LT_BANG:
      switch (*data)
        {
        case '[':
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_LT_BANG_LBRACK);
        case '-':
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_LT_BANG_MINUS);
        WHITESPACE_CASES:
          ADVANCE_CHAR;
          MAYBE_RETURN;
          goto label__LEX_LT_BANG;
        default:
          BUFFER_CLEAR;
          APPEND_BYTE (*data);
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_BANG_DIRECTIVE);
        }
    case LEX_LT_BANG_MINUS:
      switch (*data)
        {
        case '-':
          if (parser->include_comments)
            {
              if (!suppress && parser->buffer.size > 0)
                emit_text_node (parser);
            }
          dsk_buffer_clear (&parser->buffer);
          CONSUME_CHAR_AND_SWITCH_STATE (LEX_COMMENT);
          break;
        default:
          goto disallowed_char;
        }
    case LEX_COMMENT:
      {
        const char *hyphen = memchr (data, '-', len);
        if (hyphen == NULL)
          {
            if (parser->include_comments)
              dsk_buffer_append (&parser->buffer, len, data);
            parser->line_no += count_newlines (len, data);
            return DSK_TRUE;
          }
        else
          {
            unsigned skip;
            if (parser->include_comments)
              dsk_buffer_append (&parser->buffer, hyphen - data, data);
            skip = hyphen - data;
            parser->line_no += count_newlines (skip, data);
            len -= skip;
            data += skip;
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_COMMENT_MINUS);
          }
      }
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
    case LEX_BANG_DIRECTIVE:
      /* this encompasses ELEMENT, ATTLIST, ENTITY, DOCTYPE declarations */
      /* strategy: try every substring until one ends with '>' */
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


