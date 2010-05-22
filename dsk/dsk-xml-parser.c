#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <search.h>             /* for the seldom-used lsearch() */
#include "dsk.h"
#include "../gskrbtreemacros.h"

/* TODO:
   _ check for NUL
   _ beef up "directive" handling
   _ replace DskBuffer with simple array length/alloced/data
 */

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
  LEX_DEFAULT_ENTITY_REF,      /* the usual state parsing text */
  LEX_LT,                       /* just got an "<" */
  LEX_OPEN_ELEMENT_NAME,        /* in element-name of open tag */
  LEX_OPEN_IN_ATTRS,            /* waiting for attribute-name or ">" */
  LEX_OPEN_IN_ATTR_NAME,        /* in attribute name */
  LEX_OPEN_AFTER_ATTR_NAME,     /* after attribute name */
  LEX_OPEN_AFTER_ATTR_NAME_EQ,  /* after attribute name= */
  LEX_OPEN_IN_ATTR_VALUE_SQ,    /* in single-quoted attribute name */
  LEX_OPEN_IN_ATTR_VALUE_DQ,    /* in double-quoted attribute name */
  LEX_OPEN_IN_ATTR_VALUE_SQ_ENTITY_REF, /* in attribute-value, having      */
  LEX_OPEN_IN_ATTR_VALUE_DQ_ENTITY_REF, /* ...an "&" waiting for semicolon */
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
static const char *
lex_state_description (LexState state)
{
  switch (state)
    {
    case LEX_DEFAULT:                           return "the usual state parsing text";
    case LEX_DEFAULT_ENTITY_REF:               return "the usual state parsing text";
    case LEX_LT:                                return "after '<'";
    case LEX_OPEN_ELEMENT_NAME:                 return "in element-name of open tag";
    case LEX_OPEN_IN_ATTRS:                     return "waiting for attribute-name or '>'";
    case LEX_OPEN_IN_ATTR_NAME:
    case LEX_OPEN_AFTER_ATTR_NAME:
    case LEX_OPEN_AFTER_ATTR_NAME_EQ:
    case LEX_OPEN_IN_ATTR_VALUE_SQ:
    case LEX_OPEN_IN_ATTR_VALUE_DQ:
    case LEX_OPEN_IN_ATTR_VALUE_SQ_ENTITY_REF:
    case LEX_OPEN_IN_ATTR_VALUE_DQ_ENTITY_REF: return "in attributes";
    case LEX_LT_SLASH:                          return "aftert '</'";
    case LEX_CLOSE_ELEMENT_NAME:                return "in element-name of close tag";
    case LEX_AFTER_CLOSE_ELEMENT_NAME:          return "after element-name of close tag";
    case LEX_OPEN_CLOSE:                        return "got a slash after an open tag";
    case LEX_LT_BANG:
    case LEX_LT_BANG_MINUS:
    case LEX_COMMENT_MINUS:
    case LEX_COMMENT_MINUS_MINUS:
    case LEX_COMMENT:                           return "in comment";
    case LEX_LT_BANG_LBRACK:
    case LEX_LT_BANG_LBRACK_IN_CDATAHDR:
    case LEX_LT_BANG_LBRACK_CDATAHDR:           return "beginning cdata block";
    case LEX_CDATA:
    case LEX_CDATA_RBRACK:
    case LEX_CDATA_RBRACK_RBRACK:               return "in CDATA";
    case LEX_PROCESSING_INSTRUCTION:
    case LEX_PROCESSING_INSTRUCTION_QM:         return "in processing-instruction";
    case LEX_BANG_DIRECTIVE:                    return "various directives";
    }
  return NULL;
}

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

  unsigned n_transitions;
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
  unsigned passthrough_bad_ns_prefixes : 1;
  unsigned destroyed : 1;
};

/* --- parser --- */
typedef struct _NsAbbrevMap NsAbbrevMap;
struct _NsAbbrevMap
{
  char *abbrev;         /* used in source doc; NULL for default ns */
  DskXmlParserNamespaceConfig *translate;

  /* containing namespace abbreviation (that we are overriding);
     so called b/c this abbreviation is masking the one outside
     our XML element */
  NsAbbrevMap *masking;

  /* list of namespace abbreviations defined at a single element
     (owned by the stack-node) */
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

#define MAX_ENTITY_REF_LENGTH  16

typedef struct _ResultQueueNode ResultQueueNode;
struct _ResultQueueNode
{
  unsigned index;
  DskXml *xml;
  ResultQueueNode *next;
};

/* simple-buffer: a trivial one-slab buffer */
typedef struct _SimpleBuffer SimpleBuffer;
struct _SimpleBuffer
{
  uint8_t *data;
  unsigned len, alloced;
};
static inline void simple_buffer_init (SimpleBuffer *sb)
{
  sb->len = 0;
  sb->alloced = 128;
  sb->data = dsk_malloc (sb->alloced);
}
static inline void
simple_buffer_append (SimpleBuffer *sb, unsigned len, const void *data)
{
  unsigned alloced = sb->alloced;
  unsigned needed = len + sb->len;
  if (needed > alloced)
    {
      alloced += alloced;
      while (alloced < needed)
        alloced += alloced;
      sb->data = dsk_realloc (sb->data, alloced);
      sb->alloced = alloced;
    }
  memcpy (sb->data + sb->len, data, len);
  sb->len += len;
}
static inline void
simple_buffer_append_byte (SimpleBuffer *sb, uint8_t byte)
{
  if (sb->len == sb->alloced)
    {
      sb->alloced *= 2;
      sb->data = dsk_realloc (sb->data, sb->alloced);
    }
  sb->data[sb->len++] = byte;
}
static inline void
simple_buffer_clear (SimpleBuffer *sb)
{
  dsk_free (sb);
}

struct _DskXmlParser
{
  DskXmlFilename *filename;
  unsigned line_no;

  /* for text, comments, etc */
  SimpleBuffer buffer;

  /* line-no when buffer was started */
  unsigned start_line;

  /* if parsing an open tag, this is the number of attributes so far */
  unsigned n_attrs;

  NsAbbrevMap *ns_map;
  NsAbbrevMap *default_ns;

  unsigned stack_size;
  StackNode stack[MAX_DEPTH];

  char entity_buf[MAX_ENTITY_REF_LENGTH];
  unsigned entity_buf_len;

  /* the number of xml-nodes we are in the middle of constructing */
  unsigned n_to_be_returned;

  /* queue of completed xml nodes waiting to be popped off */
  ResultQueueNode *first_result, *last_result;

  LexState lex_state;

  DskXmlParserConfig *config;
};
#define COMPARE_STR_TO_NS_ABBREV_TREE(a,b,rv) rv = strcmp (a, b->abbrev)
#define COMPARE_NS_ABBREV_TREE_NODES(a,b,rv) rv = strcmp (a->abbrev, b->abbrev)
#define GET_NS_ABBREV_TREE(parser) \
  (parser)->ns_map, NsAbbrevMap *, GSK_STD_GET_IS_RED, GSK_STD_SET_IS_RED, \
  parent, left, right, COMPARE_NS_ABBREV_TREE_NODES

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
static int compare_str_to_namespace_configs (const void *a, const void *b)
{
  const char *A = a;
  const DskXmlParserNamespaceConfig *B = b;
  return strcmp (A, B->url);
}
static int compare_namespace_configs (const void *a, const void *b)
{
  const DskXmlParserNamespaceConfig *A = a;
  const DskXmlParserNamespaceConfig *B = b;
  return strcmp (A->url, B->url);
}

static int compare_str_to_parse_state_transition (const void *a, const void *b)
{
  const char *A = a;
  const ParseStateTransition *B = b;
  return strcmp (A, B->str);
}
static int compare_parse_state_transitions (const void *a, const void *b)
{
  const ParseStateTransition *A = a;
  const ParseStateTransition *B = b;
  return strcmp (A->str, B->str);
}
static int compare_uint (const void *a, const void *b)
{
  const unsigned *A = a;
  const unsigned *B = b;
  return (*A < *B) ? -1 : (*A > *B) ? 1 : 0;
}

static ParseState *
copy_parse_state (ParseState *src)
{
  ParseState *ps = dsk_malloc0 (sizeof (ParseState));
  unsigned i;
  ps->n_transitions = src->n_transitions;
  ps->transitions = dsk_malloc (sizeof (ParseStateTransition) * src->n_transitions);
  for (i = 0; i < ps->n_transitions; i++)
    {
      ps->transitions[i].str = dsk_strdup (src->transitions[i].str);
      ps->transitions[i].state = copy_parse_state (src->transitions[i].state);
    }
  ps->n_ret = src->n_ret;
  ps->ret_indices = dsk_memdup (sizeof (unsigned) * src->n_ret, src->ret_indices);
  return ps;
}

/* helper function to add all ret_indices contained in src
   to dst */
static void
union_copy_parse_state (ParseState *dst,
                        ParseState *src)
{
  unsigned i;
  for (i = 0; i < src->n_transitions; i++)
    {
      unsigned j;
      for (j = 0; j < dst->n_transitions; j++)
        if (strcmp (src->transitions[i].str, dst->transitions[j].str) == 0)
          {
            union_copy_parse_state (dst->transitions[j].state, src->transitions[i].state);
            break;
          }
      if (j == dst->n_transitions)
        {
          dst->transitions = dsk_realloc (dst->transitions, sizeof (ParseStateTransition) * (dst->n_transitions + 1));
          dst->transitions[dst->n_transitions].str = dsk_strdup (src->transitions[i].str);
          dst->transitions[dst->n_transitions].state = copy_parse_state (src->transitions[i].state);
          dst->n_transitions++;
        }
    }

  /* add return points */
  if (src->n_ret > 0)
    {
      dst->ret_indices = dsk_realloc (dst->ret_indices, sizeof (unsigned) * (src->n_ret + dst->n_ret));
      memcpy (dst->ret_indices + dst->n_ret, src->ret_indices, src->n_ret * sizeof (unsigned));
    }
}

/* Our goal is to integrate pure wildcards into the
 * state-machine.  Assuming there is a wildcard tree:  it is moved
 * into the 'wildcard_transition' member of the ParseState
 * However, it must be replicated in each non-wildcard subtree:
 * we must perform a union with each non-wildcard subtree.
 * 
 * If this state DOES NOT contain a wildcard,
 * just recurse on the transitions.
 */
static void
expand_parse_state_wildcards_recursive (ParseState *state)
{
  unsigned i;
  unsigned wc_trans;
  for (wc_trans = 0; wc_trans < state->n_transitions; wc_trans++)
    if (strcmp (state->transitions[wc_trans].str, "*") == 0)
      break;
  if (wc_trans < state->n_transitions)
    {
      /* We have a "*" node. */

      /* move transition to wildcard transition */
      state->wildcard_transition = state->transitions[i].state;

      /* union "*" subtree with each transition's subtree */
      for (i = 0; i < state->n_transitions; i++)
        if (i != wc_trans)
          union_copy_parse_state (state->transitions[i].state, state->wildcard_transition);

      /* remove "*" node from transition list */
      if (wc_trans + 1 < state->n_transitions)
        state->transitions[wc_trans] = state->transitions[state->n_transitions-1];
      if (--state->n_transitions == 0)
        {
          dsk_free (state->transitions);
          state->transitions = NULL;
        }
    }

  qsort (state->transitions, state->n_transitions, sizeof (ParseStateTransition),
         compare_parse_state_transitions);

  /* not really necessary, since we don't care if identical xml nodes are returned
     in any particular order to they various ret-indices.  predicatibility is
     slightly useful though, so may as well. */
  qsort (state->ret_indices, state->n_ret, sizeof (unsigned), compare_uint);

  /* recurse on children */
  for (i = 0; i < state->n_transitions; i++)
    expand_parse_state_wildcards_recursive (state->transitions[i].state);
  if (state->wildcard_transition != NULL)
    expand_parse_state_wildcards_recursive (state->wildcard_transition);
}

DskXmlParserConfig *
dsk_xml_parser_config_new (DskXmlParserFlags flags,
			   unsigned          n_ns,
			   const DskXmlParserNamespaceConfig *ns,
			   unsigned          n_xmlpaths,
			   char            **xmlpaths,
                           DskError        **error)
{
  /* copy and sort the namespace mapping, if enabled */
  DskXmlParserNamespaceConfig *ns_slab = NULL;
  DskXmlParserConfig *config;
  unsigned i;
  if ((flags & DSK_XML_PARSER_IGNORE_NS) == 0)
    {
      unsigned total_strlen = 0;
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
      qsort (ns_slab, n_ns, sizeof (DskXmlParserNamespaceConfig),
             compare_namespace_configs);
      for (i = 1; i < n_ns; i++)
        if (strcmp (ns_slab[i-1].url, ns_slab[i].url) == 0)
          {
            dsk_free (ns_slab);
            return NULL;
          }
    }

  config = dsk_malloc0 (sizeof (DskXmlParserConfig));
  config->ref_count = 1;
  config->ignore_ns = (flags & DSK_XML_PARSER_IGNORE_NS) ? 1 : 0;
  config->include_comments = (flags & DSK_XML_PARSER_INCLUDE_COMMENTS) ? 1 : 0;
  config->passthrough_bad_ns_prefixes = 0;
  config->n_ns = n_ns;
  config->ns = ns_slab;

  /* in phase 1, we pretend '*' is a legitimate normal path component.
     we rework the state machine in phase 2 to fix this. */
  for (i = 0; i < n_xmlpaths; i++)
    {
      char **pieces = validate_and_split_xpath (xmlpaths[i], error);
      ParseState *at;
      unsigned p;
      if (pieces == NULL)
        {
          dsk_xml_parser_config_destroy (config);
          return config;
        }
      at = &config->base;
      for (p = 0; pieces[p] != NULL; p++)
        {
          size_t tmp = at->n_transitions;
          ParseStateTransition *trans = lsearch (pieces[i], at->transitions,
                                                 &tmp,          /* wtf lsearch()? */
                                                 sizeof (ParseStateTransition),
                                                 compare_str_to_parse_state_transition);
          if (trans == NULL)
            {
              at->transitions = dsk_realloc (at->transitions, sizeof (ParseStateTransition) * (1+at->n_transitions));
              trans = &at->transitions[at->n_transitions];
              trans->str = pieces[p];
              trans->state = dsk_malloc0 (sizeof (ParseState));
              at->n_transitions += 1;
            }
          else
            dsk_free (pieces[p]);

          at = trans->state;
        }
      dsk_free (pieces);

      /* add to return-list */
      at->ret_indices = dsk_realloc (at->ret_indices, sizeof (unsigned) * (at->n_ret+1));
      at->ret_indices[at->n_ret++] = i;
    }

  /* phase 2, '*' nodes are deleted and the subtree is moved to the wildcard subtree,
     and the subtree is copied into each siblings subtree. */
  expand_parse_state_wildcards_recursive (&config->base);

#if 0
  /* dump tree */
  ...
#endif
  return config;
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

/* utf8_state:
     0: default
     1: 2-byte codes
     2,3: 3-byte codes
     4,5,6: 4-byte codes
     7: error
 */

static dsk_boolean is_valid_ascii_char (unsigned c, dsk_boolean strict)
{
  DSK_UNUSED (strict);
  return c > 32;
}
static dsk_boolean is_valid_unichar2 (unsigned c, dsk_boolean strict)
{
  DSK_UNUSED (strict);
  DSK_UNUSED (c);
  return DSK_TRUE;
}
static dsk_boolean is_valid_unichar3 (unsigned c, dsk_boolean strict)
{
  DSK_UNUSED (strict);
  if (! (c <= 0xd7ff || (0xe000 <= c && c <= 0xfffd)
        || (c > 0x10000)))
    return DSK_FALSE;
  return DSK_TRUE;
}
static dsk_boolean is_valid_unichar4 (unsigned c, dsk_boolean strict)
{
  DSK_UNUSED (strict);
  return c <= 0x10ffff;
}

static dsk_boolean utf_validate_open_element (DskXmlParser *parser,
                                              AttrOffset   *attr_offsets,
                                              DskError    **error)
{
  /* do UTF-8 validation (and other checks) */
  unsigned utf8_state = 0;
  unsigned cur;
  unsigned line_offset = 0;
  unsigned ao_i = 0;
  dsk_boolean is_strict = DSK_TRUE;
  unsigned offset = 0;
  attr_offsets[ao_i].frag = parser->buffer.first_frag;
  attr_offsets[ao_i].frag_offset = 0;
  attr_offsets[ao_i].length = 0;
  ao_i++;
  for (frag = parser->buffer.first_frag; frag != NULL; frag = frag->next)
    {
      unsigned rem = frag->buf_length;
      uint8_t *start_at = frag->buf + frag->buf_start;
      uint8_t *at = start_at;
      if (rem == 0)
        continue;
handle_next_char:
      if (utf8_state == 0)
        {
          if ((*at & 0x80) == 0)
            {
              if (*at == 0)
                {
                  attr_offsets[ao_i].frag = frag;
                  attr_offsets[ao_i].frag_offset = at - start_at;

                  /* we store the overall buffer offset in 'length',
                     then subtract once we get the next offset. */
                  attr_offsets[ao_i].length = offset;
                  attr_offsets[ao_i-1].length = offset - attr_offsets[ao_i-1].length;
                  ao_i++;

                  /* odd elements other than the first are all strict */
                  is_strict = ao_i & 1;
                }
              utf8_state = 0;
              if (!is_valid_ascii_char (*at, is_strict))
                goto bad_character;
              if (*at == '\n')
                line_offset++;
            }
          else if ((*at & 0xe0) == 0xb0)
            {
              cur = *at & 0x1f;
              utf8_state = 1;
            }
          else if ((*at & 0xf0) == 0xe0)
            {
              cur = *at & 0xf;
              utf8_state = 2;
            }
          else if ((*at & 0xf8) == 0xf0)
            {
              cur = *at & 0x7;
              utf8_state = 4;
            }
          else
            goto bad_utf8;
        }
      else
        switch (utf8_state)
          {
            /* bytes neither terminal nor initial are handled the same: */
          case 2:
          case 4:
          case 5:
            if ((*at & 0xb0) != 0x80)
              goto bad_utf8;
            cur <<= 6;
            cur |= *at & 0x3f;
            ++utf8_state;
            break;
          case 1: /* terminal byte for 2-byte code-point */
            if ((*at & 0xb0) != 0x80)
              goto bad_utf8;
            cur <<= 6;
            cur |= *at & 0x3f;
            if (cur < 0x80)
              goto bad_utf8;
            if (!is_valid_unichar2 (cur, is_strict))
              goto bad_character;
            utf8_state = 0;
            break;
          case 3: /* terminal byte for 3-byte code-point */
            if ((*at & 0xb0) != 0x80)
              goto bad_utf8;
            cur <<= 6;
            cur |= *at & 0x3f;
            if (cur < 0x800)
              goto bad_utf8;
            if (!is_valid_unichar3 (cur, is_strict))
              goto bad_character;
            utf8_state = 0;
            break;
          case 6: /* terminal byte for 4-byte code-point */
            if ((*at & 0xb0) != 0x80)
              goto bad_utf8;
            cur <<= 6;
            cur |= *at & 0x3f;
            if (cur < 0x10000)
              goto bad_utf8;
            if (!is_valid_unichar4 (cur, is_strict))
              goto bad_character;
            utf8_state = 0;
            break;
          }
      offset++;
      if (--rem != 0)
        {
          at++;
          goto handle_next_char;
        }
    }
  if (utf8_state != 0)
    goto bad_utf8;

  /* fixup the last length */
  attr_offsets[ao_i-1].length = offset - attr_offsets[ao_i-1].length;

  dsk_assert (ao_i != parser->n_attrs * 2 + 1);
  return DSK_TRUE;

bad_utf8:
  /* TODO: this doesn't count whitespace added between the attributes!!!!!! */
  dsk_set_error (error, "bad UTF-8 at %s, line %u, in open-tag",
                 parser->filename ? parser->filename->filename : "string",
                 parser->start_line + line_offset);
  return DSK_FALSE;

bad_character:
  /* TODO: this doesn't count whitespace added between the attributes!!!!!! */
  dsk_set_error (error, "bad character at %s, line %u, in open-tag",
                 parser->filename ? parser->filename->filename : "string",
                 parser->start_line + line_offset);
  return DSK_FALSE;

}

static DskXmlParserNamespaceConfig *
lookup_translation (AttrOffset *attr,
                    DskXmlParser *parser,
                    DskError    **error)
{
  char *url;
  DskXmlParserNamespaceConfig *rv;
  if (attr->length < 1024)
    url = alloca (attr->length + 1);
  else
    url = dsk_malloc (attr->length + 1);
  dsk_buffer_fragment_peek (attr->frag, attr->frag_offset, attr->length, url);
  url[attr->length] = 0;
  rv = bsearch (url, parser->config->ns, parser->config->n_ns, sizeof (DskXmlParserNamespaceConfig),
                compare_str_to_namespace_configs);
  dsk_set_error (error, "unhandled namespace URL '%s' encountered", url);
  if (attr->length >= 1024)
    dsk_free (url);
  return rv;
}

static dsk_boolean
has_ns_prefix (DskXmlParser *parser,
               AttrOffset *attr,
               AttrOffset *end_prefix_out,
               unsigned   *prefix_len_out,
               DskXmlParserNamespaceConfig **trans_out)
{
  /* search for ':' in attr */
  unsigned offset = attr->frag_offset;
  DskBufferFragment *frag = attr->frag;
  unsigned len = 0;
  while (frag)
    {
      uint8_t *start = frag->buf + frag->buf_start;
      uint8_t *at = start + offset;
      uint8_t *end = start + frag->buf_length;
      while (at < end)
        {
          if (*at == ':')
            {
              char *pref = alloca (len + 1);
              NsAbbrevMap *abbrev;
              dsk_buffer_fragment_peek (attr->frag, attr->frag_offset, len, pref);
              pref[len] = 0;
              GSK_RBTREE_LOOKUP_COMPARATOR (GET_NS_ABBREV_TREE (parser), pref, COMPARE_STR_TO_NS_ABBREV_TREE, abbrev);
              if (abbrev == NULL)
                *trans_out = NULL;
              else
                *trans_out = abbrev->translate;

              end_prefix_out->length = attr->length - len - 1;
              end_prefix_out->frag = frag;
              *prefix_len_out = len;

              return DSK_TRUE;
            }
          else if (*at == 0)
            return DSK_FALSE;
          at++;
          len++;
        }
      frag = frag->next;
      offset = 0;
    }
  dsk_return_val_if_reached (NULL, DSK_FALSE);
}

static char *
buffer_fragment_get_string (DskBufferFragment *frag,
                            unsigned           frag_offset,
                            unsigned           len)
{
  char *rv = dsk_malloc (len + 1);
  dsk_buffer_fragment_peek (frag, frag_offset, len, rv);
  rv[len] = 0;
  return rv;
}

static dsk_boolean handle_open_element (DskXmlParser *parser,
                                        DskError    **error)
{
  ParseState *cur_state = (parser->stack_size == 0) ? &parser->config->base : parser->stack[parser->stack_size-1].state;
  ParseState *new_state;
  ParseStateTransition *trans;
  unsigned n_attrs = parser->n_attrs;
  AttrOffset *attr_offsets = alloca (sizeof (AttrOffset) * n_attrs * 2);
  NsAbbrevMap *defined_list = NULL;      /* namespace defs in this element */

  /* perform UTF-8 and character validation;
     find attribute locations. */
  if (!utf_validate_open_element (parser, attr_offsets, error))
    return DSK_FALSE;

  /* do xmlns namespace translation (unless suppressed) */
  if (!parser->config->ignore_ns)
    {
      unsigned i;
      DskXmlParserNamespaceConfig **xlats = alloca (sizeof(void*) * n_attrs);
      for (i = 0; i < n_attrs; i++)
        {
          char tmp_buf[6];
          if (dsk_buffer_fragment_peek (attr_offsets[2*i+1].frag,
                                        attr_offsets[2*i+1].frag_offset,
                                        6, tmp_buf) != 6)
            {
              xlats[i] = NULL;
              continue;
            }
          if (memcmp (tmp_buf, "xmlns", 5) == 0
           && (tmp_buf[5] == 0 || tmp_buf[5] == ':'))
            {
              /* get url and see if we have a translation for it. */
              xlats[i] = lookup_translation (attr_offsets + (2*i+2), parser, error);
              if (xlats[i] == NULL)
                {
                  dsk_add_error_prefix (error, "at %s, line %u",
                                        parser->filename ? parser->filename->filename : "string",
                                        parser->start_line);
                  return DSK_FALSE;
                }
            }
        }
      for (i = 0; i < n_attrs; i++)
        if (xlats[i] != NULL)
          {
            if (attr_offsets[2*i+1].length == 5)
              {
                /* default ns */
                NsAbbrevMap *map;
                map = dsk_malloc (sizeof (NsAbbrevMap));
                map->abbrev = NULL;
                map->translate = xlats[i];

                /* add map to list for stack-node */
                map->defined_list_next = defined_list;
                defined_list = map;

                /* set/replace default ns */
                map->masking = parser->default_ns;
                parser->default_ns = map;
              }
            else
              {
                /* prefixed-namespace */
                NsAbbrevMap *map;
                DskBufferFragment *frag = attr_offsets[2*i+1].frag;
                NsAbbrevMap *existing;
                unsigned frag_offset = attr_offsets[2*i+1].frag_offset;
                unsigned abbrev_len = attr_offsets[2*i+1].length - 6;
                map = dsk_malloc (sizeof (NsAbbrevMap) + abbrev_len + 1);
                dsk_buffer_fragment_advance (&frag, &frag_offset, 6);
                map->abbrev = (char*)(map+1);
                dsk_buffer_fragment_peek (frag, frag_offset, abbrev_len, map->abbrev);
                memmove (map->abbrev, map->abbrev + 6, abbrev_len);
                map->abbrev[abbrev_len] = 0;

                /* add map to list for stack-node */
                map->defined_list_next = defined_list;
                defined_list = map;

                /* insert/replace tree node */
                GSK_RBTREE_INSERT (GET_NS_ABBREV_TREE (parser), map, existing);
                map->masking = existing;
                if (existing != NULL)
                  GSK_RBTREE_REPLACE_NODE (GET_NS_ABBREV_TREE (parser), existing, map);
              }
          }

      /* rewrite element name and attribute names to use namespace config */
      for (i = 0; i < n_attrs; i++)
        if (xlats[i] == NULL)
          {
            /* is there a namespace prefix on this attr? */
            DskXmlParserNamespaceConfig *ns_config;
            unsigned prefix_len;
            AttrOffset end_prefix;
            if (has_ns_prefix (parser, &attr_offsets[2*i+1], &end_prefix, &prefix_len, &ns_config))
              {
                if (ns_config == NULL)
                  {
                    if (!parser->config->passthrough_bad_ns_prefixes)
                      {
                        char *prefix = buffer_fragment_get_string (attr_offsets[2*i+1].frag, attr_offsets[2*i+1].frag_offset, prefix_len);
                        dsk_set_error (error,
                                       "bad namespace prefix '%s' at %s, line %u",
                                       prefix,
                                       parser->filename ? parser->filename->filename : "string",
                                       parser->start_line);
                        dsk_free (prefix);
                        return DSK_FALSE;
                      }
                  }
                else
                  {
                    /* stash away/mod data */
                    ...
                    attr_offsets[...];
                  }
              }
            else
              space += attr_offsets[2*i+1].length + 1;

            space += attr_offsets[2*i+2].length + 1;
          }
      ...
    }

  /* are we going to want to return this node to an end-user? */
  if (cur_state == NULL)
    {
      new_state = NULL;
      trans = NULL;
    }
  else
    {
      ParseStateTransition *trans = bsearch (name, cur_state->transitions, cur_state->n_transitions, sizeof (ParseStateTransition),
                                             compare_str_to_parse_state_transition);
      if (trans != NULL)
        new_state = trans->state;
      else
        new_state = cur_state->wildcard_transition;
    }
  if (new_state->n_ret > 0)
    {
      ...
    }

  /* push entry onto stack */
  if (parser->stack_size == MAX_DEPTH)
    {
      ...
    }
  StackNode *st;
  st = &parser->stack[parser->stack_size++];
  st->name = ...;
  st->state = new_state;
  st->defined_list = defined_list;
  st->n_children = 0;
  st->children = NULL;
  st->children_alloced = 0;
  if (parser->n_to_be_returned > 0)
    {
      /* include attributes */
      st->kv = attributes;
      ...
    }
  else
    {
      /* no need for attributes */
      st->kv = NULL;
      dsk_free (attributes);
    }
}

static dsk_boolean handle_close_element (DskXmlParser *parser,
                                        DskError    **error)
{
  /* do UTF-8 validation (and other checks) */
  ...

  /* do xmlns namespace translation (unless suppressed) */
  ...

  /* check that it matches the top of the stack */
  ...

  /* see if we need to return this element (either to
     an end-user or just the next node down on the stack, or both);
     construct xml node if so */
  ...

  /* push any results on the queue if needed */
  ...

  /* pop the stack */
  ...
}

static dsk_boolean handle_empty_element (DskXmlParser *parser,
                                         DskError    **error)
{
  ...
}


/* only called if we need to handle the text node (ie its in a xml element
   we are going to return) */
static void        handle_text_node          (DskXmlParser *parser);
/* only called if we need to handle the comment (ie its in a xml element
   we are going to return, and the user expresses interest in the comments) */
static void        handle_comment            (DskXmlParser *parser);


typedef enum
{
  TRY_DIRECTIVE__NEEDS_MORE,
  TRY_DIRECTIVE__SUCCESS,
  TRY_DIRECTIVE__ERROR
} TryDirectiveResult;
static TryDirectiveResult try_directive (DskXmlParser *parser,
                                         DskError    **error)
{
  /* FIXME: this does no validation at all of directives. */
  /* FIXME: we actually want to handle internal and external entities. */
  /* FIXME: we should probably handle conditionals??? */
  /* FIXME: this wont work right for directives that are nested... */
  DSK_UNUSED (parser);
  DSK_UNUSED (error);
  return TRY_DIRECTIVE__SUCCESS;
}
/* --- lexing --- */
static unsigned count_newlines (unsigned len, const char *data)
{
  unsigned rv = 0;
  while (len--)
    if (*data++ == '\n')
      rv++;
  return rv;
}
dsk_boolean
dsk_xml_parser_feed(DskXmlParser       *parser,
                    unsigned            len,
                    const char               *data,
                    DskError          **error)
{
  //dsk_boolean suppress;
#define BUFFER_CLEAR            dsk_buffer_clear (&parser->buffer)
#define APPEND_BYTE(val)        dsk_buffer_append_byte (&parser->buffer, (val))
#define MAYBE_RETURN            do{if(len == 0) return DSK_TRUE;}while(0)
#define ADVANCE_NON_NL          do{len--; data++;}while(0)
#define ADVANCE_CHAR            do{if (*data == '\n')parser->line_no++; len--; data++;}while(0)
#define ADVANCE_NL              do{parser->line_no++; len--; data++;}while(0)
#define CONSUME_CHAR_AND_SWITCH_STATE(STATE) do{ parser->lex_state = STATE; ADVANCE_CHAR; MAYBE_RETURN; goto label__##STATE; }while(0)
#define CONSUME_NL_AND_SWITCH_STATE(STATE) do{ parser->lex_state = STATE; ADVANCE_NL; MAYBE_RETURN; goto label__##STATE; }while(0)
#define CONSUME_NON_NL_AND_SWITCH_STATE(STATE) do{ parser->lex_state = STATE; ADVANCE_NON_NL; MAYBE_RETURN; goto label__##STATE; }while(0)
#define IS_SUPPRESSED           (parser->n_to_be_returned == 0)
#define CUT_TO_BUFFER           do {if (!IS_SUPPRESSED && start < data) dsk_buffer_append (&parser->buffer, data-start, start); }while(0)
#define CHECK_ENTITY_TOO_LONG(newlen)                                      \
        do { if (newlen > MAX_ENTITY_REF_LENGTH)                           \
          {                                                                \
            memcpy (parser->entity_buf + parser->entity_buf_len,           \
                    data,                                                  \
                    MAX_ENTITY_REF_LENGTH - parser->entity_buf_len);       \
            goto entity_ref_too_long;                                      \
          } } while (0)
  //suppress = parser->n_to_be_returned == 0
         //&& (parser->stack_size == 0 || parser->stack[parser->stack_size-1].state == NULL);
  MAYBE_RETURN;
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
            CONSUME_NON_NL_AND_SWITCH_STATE (LEX_DEFAULT_ENTITY_REF);
          default:
            ADVANCE_CHAR;
            if (len == 0)
              {
                CUT_TO_BUFFER;
                return DSK_TRUE;
              }
          }
      }

    case LEX_DEFAULT_ENTITY_REF:
    label__LEX_DEFAULT_ENTITY_REF:
      {
        const char *semicolon = memchr (data, ';', len);
        unsigned new_elen;
        if (semicolon == NULL)
          new_elen = parser->entity_buf_len + len;
        else
          new_elen = parser->entity_buf_len + (semicolon - data);
        CHECK_ENTITY_TOO_LONG(new_elen);
        if (IS_SUPPRESSED)
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
          parser->n_attrs = 0;
          parser->start_line = parser->line_no;
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
            ++(parser->n_attrs);
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
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_VALUE_SQ_ENTITY_REF);
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
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_OPEN_IN_ATTR_VALUE_DQ_ENTITY_REF);
          default:
            APPEND_BYTE (*data);
            ADVANCE_CHAR;
            MAYBE_RETURN;
            goto label__LEX_OPEN_IN_ATTR_VALUE_DQ;
          }
      }
    case LEX_OPEN_IN_ATTR_VALUE_SQ_ENTITY_REF:
    case LEX_OPEN_IN_ATTR_VALUE_DQ_ENTITY_REF:
    label__LEX_OPEN_IN_ATTR_VALUE_SQ_ENTITY_REF:
    label__LEX_OPEN_IN_ATTR_VALUE_DQ_ENTITY_REF:
      {
        const char *semicolon = memchr (data, ';', len);
        unsigned new_elen;
        if (semicolon == NULL)
          new_elen = parser->entity_buf_len + len;
        else
          new_elen = parser->entity_buf_len + (semicolon - data);
        CHECK_ENTITY_TOO_LONG(new_elen);
        if (IS_SUPPRESSED)
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
        if (parser->lex_state == LEX_OPEN_IN_ATTR_VALUE_SQ_ENTITY_REF)
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
          if (!IS_SUPPRESSED)
            {
              if (!handle_empty_element (parser, error))
                return DSK_FALSE;
            }
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_DEFAULT);
        default:
          goto disallowed_char;
        }
    case LEX_LT_BANG:
    label__LEX_LT_BANG:
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
    label__LEX_LT_BANG_MINUS:
      switch (*data)
        {
        case '-':
          if (parser->config->include_comments)
            {
              if (!IS_SUPPRESSED && parser->buffer.size > 0)
                handle_text_node (parser);
            }
          dsk_buffer_clear (&parser->buffer);
          CONSUME_CHAR_AND_SWITCH_STATE (LEX_COMMENT);
          break;
        default:
          goto disallowed_char;
        }
    case LEX_COMMENT:
    label__LEX_COMMENT:
      {
        const char *hyphen = memchr (data, '-', len);
        if (hyphen == NULL)
          {
            if (!IS_SUPPRESSED && parser->config->include_comments)
              dsk_buffer_append (&parser->buffer, len, data);
            parser->line_no += count_newlines (len, data);
            return DSK_TRUE;
          }
        else
          {
            unsigned skip;
            if (!IS_SUPPRESSED && parser->config->include_comments)
              dsk_buffer_append (&parser->buffer, hyphen - data, data);
            skip = hyphen - data;
            parser->line_no += count_newlines (skip, data);
            len -= skip;
            data += skip;
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_COMMENT_MINUS);
          }
      }
    case LEX_COMMENT_MINUS:
    label__LEX_COMMENT_MINUS:
      {
        switch (*data)
          {
          case '-':
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_COMMENT_MINUS_MINUS);
          default:
            APPEND_BYTE ('-');
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_COMMENT);
          }
      }
    case LEX_COMMENT_MINUS_MINUS:
    label__LEX_COMMENT_MINUS_MINUS:
      {
        switch (*data)
          {
          case '-':
            if (!IS_SUPPRESSED && parser->config->include_comments)
              APPEND_BYTE('-');
            CONSUME_NON_NL_AND_SWITCH_STATE (LEX_COMMENT_MINUS_MINUS);
          case '>':
            if (!IS_SUPPRESSED && parser->config->include_comments)
              {
                handle_comment (parser);
                BUFFER_CLEAR;
              }
            CONSUME_NON_NL_AND_SWITCH_STATE (LEX_DEFAULT);
          default:
            CONSUME_CHAR_AND_SWITCH_STATE (LEX_COMMENT);
          }
      }
    case LEX_LT_BANG_LBRACK:
    label__LEX_LT_BANG_LBRACK:
      switch (*data)
        {
        case 'c': case 'C':
          parser->entity_buf_len = 1;
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_LT_BANG_LBRACK_IN_CDATAHDR);
        default:
          goto disallowed_char;
        }
    case LEX_LT_BANG_LBRACK_IN_CDATAHDR:
    label__LEX_LT_BANG_LBRACK_IN_CDATAHDR:
      if (*data == "cdata"[parser->entity_buf_len] || *data == "CDATA"[parser->entity_buf_len])
        {
          if (++parser->entity_buf_len == 5)
            CONSUME_NON_NL_AND_SWITCH_STATE (LEX_LT_BANG_LBRACK_CDATAHDR);
          ADVANCE_NON_NL;
          MAYBE_RETURN;
          goto label__LEX_LT_BANG_LBRACK_IN_CDATAHDR;
        }
      else
        goto disallowed_char;
    case LEX_LT_BANG_LBRACK_CDATAHDR:
    label__LEX_LT_BANG_LBRACK_CDATAHDR:
      switch (*data)
        {
        WHITESPACE_CASES:
          ADVANCE_CHAR;
          MAYBE_RETURN;
          goto label__LEX_LT_BANG_LBRACK_CDATAHDR;
        case '[':
          CONSUME_CHAR_AND_SWITCH_STATE (LEX_CDATA);
        default:
          goto disallowed_char;
        }
    case LEX_CDATA:
    label__LEX_CDATA:
      {
        const char *rbracket = memchr (data, ']', len);
        if (rbracket == NULL)
          {
            if (!IS_SUPPRESSED)
              dsk_buffer_append (&parser->buffer, len, data);
            parser->line_no += count_newlines (len, data);
            return DSK_TRUE;
          }
        else
          {
            unsigned skip = rbracket - data;
            if (!IS_SUPPRESSED)
              dsk_buffer_append (&parser->buffer, skip, data);
            parser->line_no += count_newlines (skip, data);
            data = rbracket;
            len -= skip;
            CONSUME_NON_NL_AND_SWITCH_STATE (LEX_CDATA_RBRACK);
          }
      }
    case LEX_CDATA_RBRACK:
    label__LEX_CDATA_RBRACK:
      if (*data == ']')
        CONSUME_NON_NL_AND_SWITCH_STATE (LEX_CDATA_RBRACK_RBRACK);
      else
        {
          if (!IS_SUPPRESSED)
            {
              APPEND_BYTE (']');
              APPEND_BYTE (*data);
            }
          CONSUME_CHAR_AND_SWITCH_STATE (LEX_CDATA);
        }
    case LEX_CDATA_RBRACK_RBRACK:
    label__LEX_CDATA_RBRACK_RBRACK:
      switch (*data)
        {
        case ']':
          if (!IS_SUPPRESSED)
            APPEND_BYTE (']');
          ADVANCE_NON_NL;
          MAYBE_RETURN;
          goto label__LEX_CDATA_RBRACK_RBRACK;
        case '>':
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_DEFAULT);
        default:
          dsk_buffer_append (&parser->buffer, 2, "]]");
          APPEND_BYTE (*data);
          CONSUME_CHAR_AND_SWITCH_STATE (LEX_CDATA);
        }
    case LEX_BANG_DIRECTIVE:
    label__LEX_BANG_DIRECTIVE:
      /* this encompasses ELEMENT, ATTLIST, ENTITY, DOCTYPE declarations */
      /* strategy: try every substring until one ends with '>' */
      {
        const char *rangle = memchr (data, '>', len);
        if (rangle == NULL)
          {
            dsk_buffer_append (&parser->buffer, len, data);
            return DSK_TRUE;
          }
        else
          {
            unsigned append = rangle + 1 - data;
            dsk_buffer_append (&parser->buffer, append, data);
            parser->line_no += count_newlines (append, data);
            len -= append;
            data += append;
            switch (try_directive (parser, error))
              {
              case TRY_DIRECTIVE__NEEDS_MORE:
                MAYBE_RETURN;
                goto label__LEX_BANG_DIRECTIVE;
              case TRY_DIRECTIVE__SUCCESS:
                parser->lex_state = LEX_DEFAULT;
                MAYBE_RETURN;
                goto label__LEX_DEFAULT;
              case TRY_DIRECTIVE__ERROR:
                return DSK_FALSE;
              }
          }
      }
    case LEX_PROCESSING_INSTRUCTION:
    label__LEX_PROCESSING_INSTRUCTION:
      switch (*data)
        {
        case '?':
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_PROCESSING_INSTRUCTION_QM);
        default:
          ADVANCE_CHAR;
          MAYBE_RETURN;
          goto label__LEX_PROCESSING_INSTRUCTION;
        }
    case LEX_PROCESSING_INSTRUCTION_QM:
    label__LEX_PROCESSING_INSTRUCTION_QM:
      switch (*data)
        {
        case '?':
          ADVANCE_NON_NL;
          MAYBE_RETURN;
          goto label__LEX_PROCESSING_INSTRUCTION_QM;
        case '>':
          CONSUME_NON_NL_AND_SWITCH_STATE (LEX_DEFAULT);
        default:
          CONSUME_CHAR_AND_SWITCH_STATE (LEX_PROCESSING_INSTRUCTION);
        }
    }

  dsk_assert_not_reached ();

#undef MAYBE_RETURN
#undef ADVANCE_CHAR
#undef ADVANCE_NL
#undef ADVANCE_NON_NL
#undef CONSUME_CHAR_AND_SWITCH_STATE
#undef CONSUME_NL_AND_SWITCH_STATE
#undef CONSUME_NON_NL_AND_SWITCH_STATE
#undef APPEND_BYTE

disallowed_char:
  {
    char dcname[16];
    dsk_assert (len > 0);
    switch (*data)
      {
      case 0: strcpy (dcname, "nul"); break;
      case '\t': strcpy (dcname, "tab"); break;
      case '\n': strcpy (dcname, "newline"); break;
      case '\r': strcpy (dcname, "CR (\\r)"); break;
      case ' ': strcpy (dcname, "SPACE"); break;
      default:
       if (*data <= 26)
         {
           dcname[0] = '^';
           dcname[1] = *data+'A'-1;
         }
       else if (*data < 32 || *(uint8_t*)data >= 128)
         sprintf (dcname, "byte 0x%02x", *(uint8_t*)data);
       else
         {
           dcname[0] = *data;
           dcname[1] = 0;
         }
      }
    dsk_set_error (error,
                   "unexpected character %s in %s, line %u (%s)",
                   dcname,
                   parser->filename ? parser->filename->filename : "string",
                   parser->line_no,
                   lex_state_description (parser->lex_state));
    return DSK_FALSE;
  }

entity_ref_too_long:
  {
    dsk_set_error (error,
                   "entity reference &%.*s... too long in %s, line %u",
                   (int)(MAX_ENTITY_REF_LENGTH - 3), parser->entity_buf,
                   parser->filename ? parser->filename->filename : "string",
                   parser->line_no);
    return DSK_FALSE;
  }
}

static void
destruct_parse_state_recursive (ParseState *state)
{
  unsigned i;
  for (i = 0; i < state->n_transitions; i++)
    {
      destruct_parse_state_recursive (state->transitions[i].state);
      dsk_free (state->transitions[i].state);
      dsk_free (state->transitions[i].str);
    }
  dsk_free (state->transitions);
  dsk_free (state->ret_indices);
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

static DskXmlFilename *new_filename (const char *str)
{
  unsigned len = strlen (str);
  DskXmlFilename *filename = dsk_malloc (sizeof (DskXmlFilename) + len + 1);
  memcpy (filename + 1, str, len + 1);
  filename->filename = (char*)(filename+1);
  filename->ref_count = 1;
  return filename;
}

DskXmlParser *
dsk_xml_parser_new (DskXmlParserConfig *config,
                    const char         *display_filename)
{
  DskXmlParser *parser;
  dsk_assert (config != NULL);
  dsk_assert (!config->destroyed);
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
  ResultQueueNode *n = parser->first_result;
  DskXml *xml;
  if (n == NULL)
    return NULL;
  *xpath_index_out = n->index;
  xml = n->xml;
  parser->first_result = n->next;
  if (parser->first_result == NULL)
    parser->last_result = NULL;
  dsk_free (n);                 /* TO CONSIDER: recycle n? */
  return xml;
}

void
dsk_xml_parser_free(DskXmlParser       *parser)
{
  while (parser->first_result)
    {
      ResultQueueNode *n = parser->first_result;
      parser->first_result = n->next;

      dsk_xml_unref (n->xml);
      dsk_free (n);
    }
  parser->last_result = NULL;

  /* free stack */
  ///...

  /* free config */
  dsk_xml_parser_config_unref (parser->config);

  /* TODO: audit / use valgrind to ensure no leakage */

  dsk_free (parser);
}

