#include <string.h>

/* type_decl -> STRUCT opt_name LBRACE member_list RBRACE
	      | UNION opt_name opt_extends LBRACE case_list RBRACE
              | SEMICOLON ;
   type_decls -> type_decl type_decls | ;
   opt_name -> BAREWORD | ;
   member -> quantity dotted_bareword(type_ref) BAREWORD(name) SEMICOLON
   member_list = member member_list | ;
   quantity = REQUIRED | OPTIONAL | REPEATED | REQUIRED REPEATED ;
   case -> CASE BAREWORD(label) COLON BAREWORD(type) SEMICOLON
         | CASE BAREWORD(label) COLON LBRACE member_list RBRACE SEMICOLON ;
   case_list = case case_list | ;
   namespace_decl -> NAMESPACE DOTTED_BAREWORD SEMICOLON
   use_statement -> USE DOTTED_BAREWORD opt_as_clause SEMICOLON
   opt_as_clause -> AS BAREWORD | ;
   file -> namespace_decl use_statements type_decls
 */

#include "dsk.h"
#include "dsk-xml-binding-internals.h"

typedef enum
{
  /* reserved words */
  TOKEN_STRUCT,
  TOKEN_UNION,
  TOKEN_CASE,
  TOKEN_AS,
  TOKEN_USE,
  TOKEN_NAMESPACE,
  TOKEN_REQUIRED,
  TOKEN_REPEATED,
  TOKEN_OPTIONAL,

  /* naked word, not a reserved word */
  TOKEN_BAREWORD,

  /* punctuation */
  TOKEN_LBRACE,
  TOKEN_RBRACE,
  TOKEN_DOT,
  TOKEN_SEMICOLON,

} TokenType;
#define token_type_is_word(tt)   ((tt) <= TOKEN_BAREWORD)

#define LBRACE_STR  "{"
#define RBRACE_STR  "}"

typedef struct Token
{
  TokenType type;
  unsigned start, len;
  unsigned line_no;
} Token;

#define TOKENIZE_INIT_TOKEN_COUNT 32

/* NOTE: we create a sentinel token at the end just for the line-number. */
static Token *
tokenize (const char *filename,
          const char *str,
          unsigned *n_tokens_out,
          DskError **error)
{
  unsigned n = 0;
  Token *rv = dsk_malloc (sizeof (Token) * TOKENIZE_INIT_TOKEN_COUNT);
  unsigned line_no = 1;
  const char *at;
#define ADD_TOKEN(type_, start_, len_) \
  do { \
    if (n == TOKENIZE_INIT_TOKEN_COUNT || ((n) & (n-1)) == 0) \
      rv = dsk_realloc (rv, sizeof (Token) * n * 2); \
    rv[n].type = type_; rv[n].start = start_; rv[n].len = len_; rv[n].line_no = line_no; \
    n++; \
  } while(0)
#define ADD_TOKEN_ADVANCE(type_, len_) \
    do { ADD_TOKEN (type_, at - str, (len_)); at += (len_); } while(0)

  while (*at)
    {
      if (('a' <= *at && *at <= 'z')
       || ('A' <= *at && *at <= 'Z')
       || *at == '_')
        {
          const char *end = at + 1;
          TokenType type;
          if (('a' <= *end && *end <= 'z')
           || ('A' <= *end && *end <= 'Z')
           || ('0' <= *end && *end <= '9')
           || *end == '_')
            end++;
          type = TOKEN_BAREWORD;
          switch (*at)
            {
            case 's':
              if (strncmp (at, "struct", 6) == 0 && (end-at) == 6)
                type = TOKEN_STRUCT;
              break;
            case 'c':
              if (strncmp (at, "case", 4) == 0 && (end-at) == 4)
                type = TOKEN_CASE;
              break;
            case 'a':
              if (strncmp (at, "as", 2) == 0 && (end-at) == 2)
                type = TOKEN_AS;
              break;
            case 'u':
              if (strncmp (at, "use", 3) == 0 && (end-at) == 3)
                type = TOKEN_AS;
              break;
            case 'n':
              if (strncmp (at, "namespace", 9) == 0 && (end-at) == 9)
                type = TOKEN_NAMESPACE;
              break;
            }
          ADD_TOKEN_ADVANCE (type, end - at);
        }
      else if (*at == '.')
        ADD_TOKEN_ADVANCE (TOKEN_DOT, 1);
      else if (*at == ';')
        ADD_TOKEN_ADVANCE (TOKEN_SEMICOLON, 1);
      else if (*at == '{')
        ADD_TOKEN_ADVANCE (TOKEN_LBRACE, 1);
      else if (*at == '}')
        ADD_TOKEN_ADVANCE (TOKEN_RBRACE, 1);
      else if (*at == ' ' || *at == '\t')
        at++;
      else if (*at == '\n')
        {
          at++;
          line_no++;
        }
      else
        {
          dsk_set_error (error, "unexpected character %s (%s:%u)",
                         dsk_ascii_byte_name (*at),
                         filename, line_no);
          dsk_free (rv);
          return NULL;
        }
    }

  /* place sentinel */
  ADD_TOKEN_ADVANCE (0, 0);
  n--;

  *n_tokens_out = n;
  return rv;
}

typedef struct
{
  const char *str;
  unsigned    n_tokens;
  Token      *tokens;
  const char *filename;         /* for error reporting */
  DskXmlBinding *binding;
} ParseContext;

typedef struct 
{
  DskXmlBindingType *type;
  const char *type_name;
  unsigned line_no;
  unsigned orig_index;
} NewTypeInfo;

typedef struct
{
  DskXmlBindingNamespace *ns;
  char *as;
} UseStatement;

static char *
make_token_string (ParseContext *context,
                   unsigned      idx)
{
  char *rv;
  dsk_assert (idx < context->n_tokens);
  rv = dsk_malloc (context->tokens[idx].len + 1);
  memcpy (rv, context->str + context->tokens[idx].start,
          context->tokens[idx].len);
  rv[context->tokens[idx].len] = 0;
  return rv;
}

static char *
parse_dotted_bareword (ParseContext *context,
                       unsigned      start,
                       unsigned     *tokens_used_out,
                       DskError    **error)
{
  if (start >= context->n_tokens)
    {
      dsk_set_error (error, "end-of-file expecting bareword (%s:%u)",
                     context->filename, context->tokens[context->n_tokens].line_no);
      return NULL;
    }
  if (!token_type_is_word (context->tokens[start].type))
    {
      dsk_set_error (error, "expected bareword, got '%.*s' (%s:%u)",
                     context->tokens[start].len,
                     context->str + context->tokens[start].start,
                     context->filename,
                     context->tokens[start].line_no);
      return NULL;
    }
  unsigned comps;
  comps = 1;
  for (;;)
    {
      if (start + comps * 2 - 1 == context->n_tokens)
        goto done;
      if (context->tokens[start + comps * 2 - 1].type != TOKEN_DOT)
        goto done;
      if (start + comps * 2 == context->n_tokens
       || !token_type_is_word (context->tokens[start + comps * 2].type))
        {
          dsk_set_error (error, "expected bareword, got '%.*s' (%s:%u)",
                         context->tokens[start + comps*2].len,
                         context->str + context->tokens[start + comps*2].start,
                         context->filename,
                         context->tokens[start + comps*2].line_no);
          return NULL;
        }
      comps++;
    }
  *tokens_used_out = comps * 2 - 1;

done:
  {
    unsigned alloc_len, i;
    char *rv, *at;
    for (i = 0; i < comps; i++)
      alloc_len += context->tokens[start + i * 2].len + 1;
    rv = dsk_malloc (alloc_len);
    at = rv;
    for (i = 0; i < comps; i++)
      {
        memcpy (at, context->str + context->tokens[start + i*2].start,
                context->tokens[start + i*2].len);
        at += context->tokens[start + i*2].len;
        *at++ = '.';
      }
    --at;
    *at = 0;
    return rv;
  }
}

static dsk_boolean
parse_member_list (ParseContext *context,
                   unsigned      lbrace_index,
                   unsigned     *n_tokens_used_out,
                   unsigned     *n_members_out,
                   DskXmlBindingStructMember **members_out,
                   DskError    **error)
{
  unsigned at = lbrace_index;
  if (context->tokens[at].type != TOKEN_LBRACE)
    {
      dsk_set_error (error, "expected '"LBRACE_STR"' after struct NAME (%s:%u)",
                     context->filename, context->tokens[at].line_no);
      return DSK_FALSE;
    }

  /* maximum possible members is the number of ';' + 1 */
  unsigned max_members;
  max_members = 1;
  for (at = lbrace_index + 1; at < context->n_tokens && context->tokens[at].type != TOKEN_RBRACE; at++)
    if (context->tokens[at].type == TOKEN_SEMICOLON)
      ++max_members;

  /* parse each member */
  unsigned n_members;
  n_members = 0;
  DskXmlBindingStructMember *members;
  members = dsk_malloc (sizeof(DskXmlBindingStructMember *) * max_members);
  while (at < context->n_tokens
      && context->tokens[at].type != TOKEN_RBRACE)
    {
      DskXmlBindingStructMember member;
      if (context->tokens[at].type == TOKEN_SEMICOLON)
        {
          at++;
          continue;
        }

      /* parse type */
      char *bw;
      unsigned used;
      bw = parse_dotted_bareword (context, at, &used, error);
      if (bw == NULL)
        goto got_error;
      type = dotted_bareword_to_type (context, bw, error);
      if (type == NULL)
        goto got_error;
      at += used;

      /* parse quantity characters: * ? ! + */
      if (at == context->n_tokens)
        {
          dsk_set_error (error, "too few tokens for structure member (%s:%u)",
                         context->filename, context->tokens[at].line_no);
          goto got_error;
        }
      switch (context->tokens[at].type)
        {
        case TOKEN_EXCLAMATION_POINT:
          member.quantity = DSK_XML_BINDING_REQUIRED;
          break;
        case TOKEN_QUESTION_MARK:
          member.quantity = DSK_XML_BINDING_OPTIONAL;
          break;
        case TOKEN_ASTERISK:
          member.quantity = DSK_XML_BINDING_REPEATED;
          break;
        case TOKEN_PLUS:
          member.quantity = DSK_XML_BINDING_REQUIRED_REPEATED;
          break;
        default:
          dsk_set_error (error, "expected '+', '!', '*' or '?', got %.*s at (%s:%u)",
                         context->tokens[at].len, context->str + context->tokens[at].start,
                         context->filename,
                         context->tokens[at].line_no);
          goto got_error;
        }

      /* parse name */
      if (token_type_is_word (context->tokens[at].type))
        {
          ...
        }

      /* add member */
      members[n_members++] = member;
    }
  if (at == context->n_tokens)
    {
      dsk_set_error (error, "unexpected EOF, looking for end of structure members starting %s:%u",
                     context->filename, context->tokens[lbrace_index].line_no);
      goto got_error;
    }
  *members_out = members;
  *n_members_out = n_members;
  return DSK_TRUE;

got_error:
  dsk_free (members);
  return DSK_FALSE;
}

static DskXmlBindingNamespace *
parse_file (ParseContext *context,
            DskError  **error)
{
  unsigned n_tokens = context->n_tokens;
  Token *tokens = context->tokens;
  unsigned n_tokens_used;
  if (n_tokens == 0 || tokens[0].type == TOKEN_NAMESPACE)
    {
      dsk_set_error (error,
                     "file must begin with 'namespace' (%s:%u)",
                     context->filename, tokens[0].line_no);
      return DSK_FALSE;
    }

  char *ns_name;
  ns_name = parse_dotted_bareword (context, 1, &n_tokens_used, error);
  if (ns_name == NULL)
    return DSK_FALSE;
  unsigned at;
  at = 1 + n_tokens_used;
  if (at == n_tokens || tokens[at].type != TOKEN_SEMICOLON)
    {
      dsk_set_error (error,
                     "expected ; after namespace %s (%s:%u)",
                     ns_name, context->filename, tokens[at].line_no);
      dsk_free (ns_name);
      return DSK_FALSE;
    }
  at++;         /* skip semicolon */

  unsigned n_ns_types = 0;
  NewTypeInfo *ns_type_info = NULL;
  unsigned n_use_statements = 0;
  UseStatement *use_statements = NULL;

  /* parse 'use' statements */
  while (at < n_tokens)
    {
      if (tokens[at].type == TOKEN_USE)
        {
          char *use;
          UseStatement stmt;
          unsigned use_at = at;
          use = parse_dotted_bareword (context, at + 1, &n_tokens_used, error);
          if (use == NULL)
            goto error_cleanup;
          at += 1 + n_tokens_used;
          stmt.ns = dsk_xml_binding_get_ns (context->binding, use, error);
          if (stmt.ns == NULL)
            {
              /* append suffix */
              dsk_add_error_prefix (error, "in %s, line %u:\n\t",
                                    context->filename, 
                                    context->tokens[use_at].line_no);
              dsk_free (use);
              goto error_cleanup;
            }
          if (at < n_tokens && tokens[at].type == TOKEN_AS)
            {
              if (at + 1 == n_tokens || !token_type_is_word (tokens[at+1].type))
                {
                  dsk_set_error (error,
                                 "error parsing shortname from 'as' clause (%s:%u)",
                                 context->filename, tokens[at+1].line_no);
                  dsk_free (use);
                  goto error_cleanup;
                }
              stmt.as = make_token_string (context, at + 1);
              at += 2;
            }
          else
            stmt.as = NULL;
          if (at >= n_tokens || tokens[at].type != TOKEN_SEMICOLON)
            {
              /* missing semicolon */
              dsk_set_error (error,
                             "expected ; after use %s (%s:%u)",
                             use, context->filename,
                             tokens[at].line_no);
              dsk_free (use);
              goto error_cleanup;
            }

          at++;         /* skip ; */
          dsk_free (use);

          /* store into namespace array */
          if ((n_use_statements & (n_use_statements-1)) == 0)
            {
              /* resize */
              unsigned new_size = n_use_statements ? n_use_statements*2 : 1;
              unsigned new_size_bytes = new_size * sizeof (UseStatement);
              use_statements = dsk_realloc (use_statements, new_size_bytes);
            }
          use_statements[n_use_statements++] = stmt;
        }
      else if (tokens[at].type != TOKEN_STRUCT
            && tokens[at].type != TOKEN_UNION)
        {
          dsk_set_error (error,
                         "expected 'use', 'struct' or 'union', got '%.*s' (%s:%u)",
                         tokens[at].len, context->str + tokens[at].start,
                         context->filename, tokens[at].line_no);
          return DSK_FALSE;
        }
      else
        break;          /* fall-through to struct/union handling */
    }

  /* parse type-decls */
  while (at < n_tokens)
    {
      dsk_boolean has_type_info = DSK_FALSE;
      NewTypeInfo type_info;
      type_info.line_no = tokens[at].line_no;
      type_info.orig_index = n_ns_types;

      if (tokens[at].type == TOKEN_STRUCT)
        {
          char *struct_name;
          if (at + 1 == n_tokens || !token_type_is_word (tokens[at+1].type))
            {
              dsk_set_error (error, "error parsing name of struct (%s:%u)",
                             context->filename, tokens[at+1].line_no);
              goto error_cleanup;
            }
          if (!parse_member_list (context, at + 2, &n_tokens_used,
                                  &n_members, &members, error))
            goto error_cleanup;
          struct_name = make_token_string (context, at+1);
          type = dsk_xml_binding_type_struct_new (struct_name,
                                                  n_members, members,
                                                  error);
          dsk_free (struct_name);
          free_members (n_members, members);

          if (type == NULL)
            goto error_cleanup;

          /* add to array of types */
          has_type_info = DSK_TRUE;
          type_info.type = type;
          type_info.type_name = ((DskXmlBindingTypeStruct*)type)->name;
          at += 2 + n_tokens_used;
        }
      else if (tokens[at].type == TOKEN_UNION)
        {
          char *union_name;
          if (at + 1 == n_tokens || !token_type_is_word (tokens[at+1].type))
            {
              dsk_set_error (error, "error parsing name of union (%s:%u)",
                             context->filename, tokens[at+1].line_no);
              goto error_cleanup;
            }
          if (!parse_case_list (context, at + 2, &n_tokens_used,
                                  &n_cases, &cases, error))
            {
              goto error_cleanup;
            }
          union_name = make_token_string (context, at+1);
          type = dsk_xml_binding_type_union_new (union_name, n_cases, cases, error);
          dsk_free (union_name);
          free_cases (n_cases, cases);

          if (type == NULL)
            goto error_cleanup;

          /* add to array of types */
          type_info.type = type;
          type_info.type_name = ((DskXmlBindingTypeUnion*)type)->name;
          has_type_info = DSK_TRUE;
          at += 2 + n_tokens_used;
        }
      else if (tokens[at].type == TOKEN_SEMICOLON)
        {
          at++;
        }
      else
        {
          dsk_set_error (error,
                         "unexpected token '%.*s' (%s:%u) - expected struct or union",
                         tokens[at].len, context->str + tokens[at].start,
                         context->filename, tokens[at].line_no);
          goto error_cleanup;
        }

      /* Append type-info to array, if needed */
      if (has_type_info)
        {
          if ((n_ns_types & (n_ns_types-1)) == 0)
            {
              /* resize */
              unsigned new_size = n_ns_types ? n_ns_types*2 : 1;
              unsigned new_size_bytes = new_size * sizeof (NewTypeInfo);
              ns_type_info = dsk_realloc (ns_type_info, new_size_bytes);
            }
          ns_type_info[n_ns_types++] = type_info;
        }
    }

  qsort (ns_type_info, n_ns_types, sizeof (NewTypeInfo),
         compare_new_type_info_by_name);
  for (i = 1; i < n_ns_types; i++)
    if (strcmp (ns_type_info[i-1].name, ns_type_info[i].name) == 0)
      {
        dsk_set_error (error, "namespace %s defined type %s twice (at least) (%s:%u and %s:%u)",
                       ns_name, ns_type_info[i].name,
                       context->filename, ns_type_info[i-1].line_no,
                       context->filename, ns_type_info[i].line_no);
        goto error_cleanup;
      }

  /* Create namespace (incl by-name sorted table) */
  ...

  return DSK_TRUE;

error_cleanup:
  dsk_free (ns_name);
  for (i = 0; i < n_ns_types; i++)
    dsk_xml_binding_type_unref (ns_type_info[i].type);
  dsk_free (ns_type_info);
  for (i = 0; i < n_use_statements; i++)
    dsk_free (use_statements[i].as);
  dsk_free (use_statements);
  return DSK_FALSE;
}

DskXmlBindingNamespace *
_dsk_xml_binding_parse_ns_str (binding, path, contents, error))
