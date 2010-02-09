#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "dang.h"
#include "gskrbtreemacros.h"

#define LBRACE '{'
#define RBRACE '}'

/* Map from name => DangLiteralTokenizer */
static DangLiteralTokenizer *literal_tokenizer_tree = NULL;
#define COMPARE_LITERAL_TREE_NODES(a,b, rv) \
  rv = strcmp ((a)->name, (b)->name)
#define GET_LITERAL_TOKENIZER_TREE() \
  literal_tokenizer_tree, DangLiteralTokenizer *, \
  GSK_STD_GET_IS_RED, GSK_STD_SET_IS_RED, \
  lt_parent, lt_left, lt_right, \
  COMPARE_LITERAL_TREE_NODES
#define COMPARE_STR_TO_LIT_TOKENIZER(str,b, rv) \
  rv = strcmp ((str), (b)->name)

struct _DangTokenizer
{
  DangCodePosition cp;
  unsigned n_tokens, first_token, queue_size;
  DangToken **token_queue;
  DangUtilArray data;
  unsigned brace_terminated : 1;
  unsigned got_terminal_brace : 1;
  unsigned in_interpolated_string : 1;
  unsigned in_trailing_comment : 1;
  unsigned in_c_comment : 1;
  unsigned brace_balance;

  /* For when in_interpolated_string is TRUE */
  DangUtilArray istring_buf;
  DangUtilArray istring_pieces;
  DangTokenizer *istring_subtokenizer;

  /* For when parsing a literal token using a specialized tokenizer */
  DangLiteralTokenizer *lit_tokenizer;
  void *lit_tokenizer_state;
  DangCodePosition lit_tokenizer_cp;
};

DangTokenizer *dang_tokenizer_new       (DangString     *filename)
{
  DangTokenizer *rv = dang_new (DangTokenizer, 1);
  rv->cp.filename = dang_string_ref_copy (filename);
  rv->cp.line = 1;
  rv->n_tokens = rv->first_token = 0;
  rv->queue_size = 32;
  rv->token_queue = dang_new0 (DangToken *, rv->queue_size);
  DANG_UTIL_ARRAY_INIT (&rv->data, char);
  rv->brace_terminated = 0;
  rv->got_terminal_brace = 0;
  rv->brace_balance = 0;
  rv->in_trailing_comment = 0;
  rv->in_c_comment = 0;

  rv->lit_tokenizer_state = NULL;
  rv->lit_tokenizer = NULL;
  rv->in_interpolated_string = 0;
  rv->istring_subtokenizer = NULL;

  return rv;
}

typedef enum
{
  TOKENIZE_RESULT_SUCCESS,
  TOKENIZE_RESULT_NEEDS_MORE_DATA,
  TOKENIZE_RESULT_ERROR
} TokenizeResult;

static dang_boolean
is_valid_char_to_follow_number (char c)
{
  if (c == '.')
    return FALSE;
  return (isspace (c) || ispunct (c));
}

static DangValueType *
guess_int_type (dang_boolean has_sign,
                uint64_t     value)
{
  if (has_sign)
    {
      if (value <= 0x80000000ULL)
        return dang_value_type_int32 ();
      else if (value >= 0x8000000000000000ULL)
        return NULL;
      else
        return dang_value_type_int64 ();
    }
  else
    {
      if (value < 0x80000000ULL)
        return dang_value_type_int32 ();
      else if (value >= 0x8000000000000000ULL)
        return dang_value_type_uint64 ();
      else
        return dang_value_type_int64 ();
    }
}

#define ENSURE_HAS_MORE_DATA()  \
  do{ if (at==end) return TOKENIZE_RESULT_NEEDS_MORE_DATA; }while(0)
static TokenizeResult
parse_numeric_literal (char *start,
                       char *end,
                       char **end_num_out,
                       DangValueType **type_out,
                       void **value_out,
                       DangError **error)
{
  char *at = start;
  char *end_num;
  const char *qual = NULL;
  dang_boolean has_sign = FALSE;
  dang_boolean is_float = FALSE;
  DangValueType *force_type = NULL;       /* as given by type-specifiers */
  DangValueType *type;
  char *strto_end;
  dang_assert (start < end);
  if (*at == '-')
    {
      has_sign = TRUE;
      at++;
    }
  if (*at == '0')
    {
      /* octal or hex */
      at++;
      ENSURE_HAS_MORE_DATA ();
      if (*at == 'x')
        {
          /* Scan hexidecimal characters */
          at++; /* skip x */
          ENSURE_HAS_MORE_DATA ();
          while (at < end && isxdigit (*at))
            at++;
          ENSURE_HAS_MORE_DATA ();
          if (*at == 'U' || *at == 'L' || *at == 'T' || *at == 'S' || *at == 'I' || *at == 'F' || *at =='D')
            goto got_type_specifier;
          if (!is_valid_char_to_follow_number (*at))
            goto garbage_after_number;
          end_num = at;
        }
      else if (*at == '.')
        {
          goto got_decimal_point;
        }
      else if (*at == 'e' || *at == 'E')
        goto got_float_suffix;
      else if (*at == 'U' || *at == 'L' || *at == 'T' || *at == 'S' || *at == 'I' || *at == 'F' || *at =='D')
        goto got_type_specifier;
      else
        {
          /* Scan octal characters */
          while (at < end && ('0' <= *at && *at < '8'))
            at++;
          ENSURE_HAS_MORE_DATA ();
          if (!is_valid_char_to_follow_number (*at))
            goto garbage_after_number;
          end_num = at;
        }
      goto handle_int;
    }
  else
    {
      while (at < end && isdigit (*at))
        at++;
    }
  ENSURE_HAS_MORE_DATA ();
  if (*at == '.')
    goto got_decimal_point;
  else if (*at == 'e' || *at == 'E')
    goto got_float_suffix;
  else if (*at == 'U' || *at == 'L' || *at == 'T' || *at == 'S' || *at == 'I' || *at == 'F' || *at =='D')
    goto got_type_specifier;
  else if (!is_valid_char_to_follow_number (*at))
    goto garbage_after_number;
  else
    {
      goto handle_int;
    }

got_decimal_point:
  at++;
  is_float = TRUE;
  ENSURE_HAS_MORE_DATA ();
  while (at < end && isdigit (*at))
    at++;
  if (*at == 'e' || *at == 'E')
    goto got_float_suffix;
  else if (*at == 'F' || *at == 'D')
    goto got_type_specifier;
  else if (!is_valid_char_to_follow_number (*at))
    goto garbage_after_number;
  else
    goto handle_float;

got_float_suffix:
  /* parse e32 e-32 e+32 */
  is_float = TRUE;
  at++;
  ENSURE_HAS_MORE_DATA ();
  if (*at == '+' || *at == '-')
    {
      at++;
      ENSURE_HAS_MORE_DATA ();
    }
  if (!isdigit (*at))
    {
      dang_set_error (error,
                      "expected digit after 'e' in floating-point number");
      return TOKENIZE_RESULT_ERROR;
    }
  at++;
  while (at < end && isdigit (*at))
    at++;
  ENSURE_HAS_MORE_DATA ();
  if (*at == 'F' || *at == 'D')
    goto got_type_specifier;
  goto handle_float;

got_type_specifier:
  qual = at;
  if (*at == 'F')
    {
      force_type = dang_value_type_float ();
      at++;
      goto handle_float;
    }
  else if (*at == 'D')
    {
      force_type = dang_value_type_double ();
      at++;
      goto handle_float;
    }
  else if (*at == 'U')
    {
      dang_assert (!is_float);
      at++;
      ENSURE_HAS_MORE_DATA ();
      if (*at == 'T')
        {
          force_type = dang_value_type_uint8 ();
          at++;
        }
      else if (*at == 'S')
        {
          force_type = dang_value_type_uint16 ();
          at++;
        }
      else if (*at == 'I')
        {
          force_type = dang_value_type_uint32 ();
          at++;
        }
      else if (*at == 'L')
        {
          force_type = dang_value_type_uint64 ();
          at++;
        }
      else
        {
          force_type = dang_value_type_uint32 ();
        }
      ENSURE_HAS_MORE_DATA ();
      goto handle_int;

    }
  else if (*at == 'T')
    {
      dang_assert (!is_float);
      force_type = dang_value_type_int8 ();
      at++;
      goto handle_int;
    }
  else if (*at == 'S')
    {
      dang_assert (!is_float);
      force_type = dang_value_type_int16 ();
      at++;
      goto handle_int;
    }
  else if (*at == 'I')
    {
      dang_assert (!is_float);
      force_type = dang_value_type_int32 ();
      at++;
      goto handle_int;
    }
  else if (*at == 'L')
    {
      dang_assert (!is_float);
      force_type = dang_value_type_int64 ();
      at++;
      goto handle_int;
    }
  else
    {
      dang_set_error (error,
                      "error parsing type-qualifier %c", *at);
      return TOKENIZE_RESULT_ERROR;
    }

handle_int:
  ENSURE_HAS_MORE_DATA ();
  if (!is_valid_char_to_follow_number (*at))
    goto garbage_after_number;

  /* parse uint64 with sign separate */
  {
    uint64_t uval;
    uval = strtoull (start + (has_sign ? 1 : 0), &strto_end, 0);
    if (strto_end != (qual ? qual : at))
      {
        dang_set_error (error,
                        "strtoull() did not agree on the end-of-string (%p v %p)",
                        strto_end, (qual ? qual : at));
        return TOKENIZE_RESULT_ERROR;
      }
    if (force_type != NULL)
      type = force_type;
    else
      type = guess_int_type (has_sign, uval);
    if (type == NULL)
      {
        dang_set_error (error, "cannot get a good int-type for this");
        return TOKENIZE_RESULT_ERROR;
      }
    *type_out = type;
    if (type == dang_value_type_uint8 ()
     || type == dang_value_type_int8 ())
      {
        int8_t *v = dang_new (int8_t, 1);
        *v = uval;
        if (has_sign)
          *v = -(*v);
        *value_out = v;
      }
    else if (type == dang_value_type_uint16 ()
     || type == dang_value_type_int16 ())
      {
        int16_t *v = dang_new (int16_t, 1);
        *v = uval;
        if (has_sign)
          *v = -(*v);
        *value_out = v;
      }
    else if (type == dang_value_type_uint32 ()
          || type == dang_value_type_int32 ())
      {
        int32_t *v = dang_new (int32_t, 1);
        *v = uval;
        if (has_sign)
          *v = -(*v);
        *value_out = v;
      }
    else if (type == dang_value_type_uint64 ()
          || type == dang_value_type_int64 ())
      {
        int64_t *v = dang_new (int64_t, 1);
        *v = uval;
        if (has_sign)
          *v = -(*v);
        *value_out = v;
      }
    else
      dang_assert_not_reached ();
    *end_num_out = at;
    return TOKENIZE_RESULT_SUCCESS;
  }

handle_float:
  {
    double f;
    if (force_type == NULL)
      type = dang_value_type_double ();
    else
      type = force_type;
    f = strtod (start, &strto_end);
    if (strto_end != (qual ? qual : at))
      {
        dang_set_error (error,
                        "strtod() did not agree on the end-of-string (%p v %p)",
                        strto_end, (qual ? qual : at));
        return TOKENIZE_RESULT_ERROR;
      }
    *type_out = type;
    if (type == dang_value_type_double ())
      {
        double *v = dang_new (double, 1);
        *v = f;
        *value_out = v;
      }
    else if (type == dang_value_type_float ())
      {
        float *v = dang_new (float, 1);
        *v = f;
        *value_out = v;
      }
    else
      dang_assert_not_reached ();

    *end_num_out = at;
    return TOKENIZE_RESULT_SUCCESS;
  }

garbage_after_number:
  dang_set_error (error, "unexpected character '%c' after number", *at);
  return TOKENIZE_RESULT_ERROR;
}

static TokenizeResult
parse_bareword        (char *start,
                       char *end,
                       char **end_num_out,
                       char **name_out,
                       DangError **error)
{
  char *at = start;
  dang_assert (start < end);
  DANG_UNUSED (error);
  while (isalnum (*at) || *at == '_')
    {
      at++;
      if (at == end)
        return TOKENIZE_RESULT_NEEDS_MORE_DATA;
    }
  *name_out = dang_strndup (start, at - start);
  *end_num_out = at;
  return TOKENIZE_RESULT_SUCCESS;
}

static inline dang_boolean
is_single_char_op (char c)
{
  static unsigned char bytes[32] = {
#include "single-char-ops.inc" /* generated by rule in makefile */
  };
  unsigned char b = c;
  return (bytes[b / 8] & (1 << (b & 7))) != 0;
}

static inline dang_boolean
is_multi_char_op (char c)
{
  static unsigned char bytes[32] = {
#include "multi-char-ops.inc" /* generated by rule in makefile */
  };
  unsigned char b = c;
  return (bytes[b / 8] & (1 << (b & 7))) != 0;
}


static TokenizeResult
parse_operator        (char *start,
                       char *end,
                       char **end_op_out,
                       char **op_str_out,
                       DangError **error)
{
  char *at = start;
  DANG_UNUSED (error);

  /* single character operators */
  if (is_single_char_op (*at))
    {
      *op_str_out = dang_strndup (at, 1);
      *end_op_out = at + 1;
      return TOKENIZE_RESULT_SUCCESS;
    }

  while (at < end && is_multi_char_op (*at))
    at++;
  if (at == end)
    return TOKENIZE_RESULT_NEEDS_MORE_DATA;
  else
    {
      *end_op_out = at;
      *op_str_out = dang_strndup (start, at - start);
      return TOKENIZE_RESULT_SUCCESS;
    }
}

static void
push_token (DangTokenizer *tokenizer,
            DangToken     *token)
{
  unsigned dst;
  if (token->any.code_position.filename == NULL)
    dang_code_position_copy (&token->any.code_position, &tokenizer->cp);
  if (tokenizer->n_tokens == tokenizer->queue_size)
    {
      unsigned new_q_size = tokenizer->queue_size * 2;
      if (tokenizer->first_token + tokenizer->n_tokens <= tokenizer->queue_size)
        {
          /* realloc */
          tokenizer->token_queue
            = dang_realloc (tokenizer->token_queue,
                            sizeof(DangToken*) * new_q_size);
          tokenizer->queue_size = new_q_size;
        }
      else
        {
          /* create and copy */
          DangToken **new_q = dang_new (DangToken *, new_q_size);
          unsigned p1 = (tokenizer->queue_size - tokenizer->first_token);
          unsigned p2 = (tokenizer->n_tokens - p1);
          memcpy (new_q,
                  tokenizer->token_queue + tokenizer->first_token,
                  p1 * sizeof (DangToken*));
          memcpy (new_q + p1,
                  tokenizer->token_queue,
                  p2 * sizeof (DangToken*));
          tokenizer->first_token = 0;
          dang_free (tokenizer->token_queue);
          tokenizer->token_queue = new_q;
          tokenizer->queue_size = new_q_size;
        }
    }
  dst = tokenizer->first_token + tokenizer->n_tokens;
  if (dst >= tokenizer->queue_size)
    dst -= tokenizer->queue_size;
  tokenizer->token_queue[dst] = token;
  tokenizer->n_tokens++;
}

static inline void
maybe_clear_istring_buf (DangTokenizer *tokenizer)
{
  DangTokenInterpolatedPiece piece;
  if (tokenizer->istring_buf.len == 0)
    return;
  piece.type = DANG_TOKEN_INTERPOLATED_PIECE_STRING;
  dang_code_position_copy (&piece.code_position,
                           &tokenizer->cp);
  piece.info.string = dang_strndup (tokenizer->istring_buf.data,
                                    tokenizer->istring_buf.len);
  dang_util_array_append (&tokenizer->istring_pieces, 1, &piece);
  dang_util_array_set_size (&tokenizer->istring_buf, 0);
}

static TokenizeResult
parse_escape_seq (char *str,
                  char *end,
                  char **end_out,
                  DangUtilArray *out,
                  DangError **error)
{
  static const char pairs[] = "n\n"
                              "r\r"
                              "t\t"
                              "a\a"
                              "\"\""
                              "\\\\";
  unsigned i;
  unsigned octal_val;
  for (i = 0; i < DANG_N_ELEMENTS (pairs); i += 2)
    if (str[0] == pairs[i])
      {
        *end_out = str + 1;
        dang_util_array_append (out, 1, pairs + i + 1);
        return TOKENIZE_RESULT_SUCCESS;
      }
  octal_val = 0;
  for (i = 0; i < 8 && str + i < end; i++)
    if (!('0' <= str[i] && str[i] < '8'))
      break;
    else
      {
        octal_val <<= 3;
        octal_val += (str[i] - '0');
      }
  if (i > 0)
    {
      char utf8[6];
      dang_unichar v = octal_val;
      unsigned n_bytes;
      if (str + i == end)
        return TOKENIZE_RESULT_NEEDS_MORE_DATA;
      n_bytes = dang_utf8_encode (v, utf8);
      dang_util_array_append (out, n_bytes, utf8);
      *end_out = str + i;
      return TOKENIZE_RESULT_SUCCESS;
    }
  dang_set_error (error, "unexpected char '%c' after \\", *str);
  return TOKENIZE_RESULT_ERROR;
}

static TokenizeResult
parse_char_literal (char *at,
                    char *end,
                    char **end_lit_out,
                    dang_unichar *char_out,
                    DangError **error)
{
  if (at + 3 > end)
    return TOKENIZE_RESULT_NEEDS_MORE_DATA;
  if (at[1] != '\\' && ((unsigned char)at[1]) < 128)
    {
      *char_out = at[1];
      *end_lit_out = at + 3;
      return TOKENIZE_RESULT_SUCCESS;
    }
  if (at[1] != '\\')
    {
      /* higher utf8 char */
      DangError *e = NULL;
      char *cur_at = at + 1;;
      if (!dang_utf8_scan_char (&cur_at, end - cur_at, char_out, &e))
        {
          if (e)
            {
              *error = e;
              return TOKENIZE_RESULT_ERROR;
            }
          else
            return TOKENIZE_RESULT_NEEDS_MORE_DATA;
        }
      if (cur_at == end)
        return TOKENIZE_RESULT_NEEDS_MORE_DATA;
      if (*cur_at != '\'')
        {
          dang_set_error (error, "unexpected char '%c' in character literal",
                          *cur_at);
          return TOKENIZE_RESULT_ERROR;
        }
      *end_lit_out = cur_at + 1;
      return TOKENIZE_RESULT_SUCCESS;
    }
  else
    {
      /* escape sequence */
      static const char pairs[] = "n\n"
                                  "r\r"
                                  "t\t"
                                  "a\a"
                                  "\\\\"
                                  "''";
      unsigned i;
      for (i = 0; i < DANG_N_ELEMENTS (pairs); i += 2)
        if (at[2] == pairs[i])
          {
            if (at + 3 == end)
              return TOKENIZE_RESULT_NEEDS_MORE_DATA;
            if (at[3] != '\'')
              {
                dang_set_error (error,
                    "unexpected char '%c' after \\%c in character literal",
                                at[3], at[2]);
                return TOKENIZE_RESULT_ERROR;
              }
            *char_out = pairs[i + 1];
            *end_lit_out = at + 4;
            return TOKENIZE_RESULT_SUCCESS;
          }

      if (isdigit (at[2]))
        {
          /* octal character literal */
          char obuf[9];
          unsigned n_obuf = 0;
          while (at + 2 + n_obuf < end && at[2 + n_obuf] != '\'' && n_obuf < sizeof(obuf)-1)
            {
              obuf[n_obuf] = at[2 + n_obuf];
              n_obuf++;
            }
          if (at + 2 + n_obuf >= end)
            return TOKENIZE_RESULT_NEEDS_MORE_DATA;
          if (at[2 + n_obuf] != '\'')
            {
              dang_set_error (error,
                  "unexpected char '%c' after octal in character literal",
                              at[n_obuf]);
              return TOKENIZE_RESULT_ERROR;
            }
          obuf[n_obuf] = 0;
          *char_out = strtoul (obuf, NULL, 8);
          *end_lit_out = at + 2 + n_obuf + 1;
          return TOKENIZE_RESULT_SUCCESS;
        }
      else
        {
          dang_set_error (error,
                          "unexpected char '%c' after \\ in character literal",
                          at[2]);
          return TOKENIZE_RESULT_ERROR;
        }
    }
}

static void
add_position_prefix (DangTokenizer *tokenizer,
                     DangError    **error)
{
  DangError *e = dang_error_new ("%s:%u: %s",
                                 tokenizer->cp.filename->str,
                                 tokenizer->cp.line,
                                 (*error)->message);
  dang_error_unref (*error);
  *error = e;
}

dang_boolean
dang_tokenizer_feed      (DangTokenizer  *tokenizer,
			  unsigned        len,
			  char           *str,
			  DangError     **error)
{
  char *parse_at, *parse_end;
  DangToken *token;
  if (tokenizer->istring_subtokenizer != NULL)
    {
      if (!dang_tokenizer_feed (tokenizer->istring_subtokenizer,
                                len, str, error))
        {
          return FALSE;
        }
      if (tokenizer->istring_subtokenizer->got_terminal_brace)
        {
          /* Transfer data back into tokenizer->data */
          DangUtilArray tmp;
          dang_assert (tokenizer->data.len == 0);
          tmp = tokenizer->data;
          tokenizer->data = tokenizer->istring_subtokenizer->data;
          tokenizer->istring_subtokenizer->data = tmp;
          dang_tokenizer_free (tokenizer->istring_subtokenizer);
          tokenizer->istring_subtokenizer = NULL;
        }
    }
  else
    {
      dang_util_array_append (&tokenizer->data, len, str);
      parse_at = tokenizer->data.data;
      parse_end = parse_at + tokenizer->data.len;
    }

restart:
  if (tokenizer->in_interpolated_string)
    {
      while (parse_at < parse_end)
        {
          if (*parse_at == '$')
            {
              /* Add maybe literal piece */
              maybe_clear_istring_buf (tokenizer);
              if (parse_at + 1 == parse_end)
                goto done;
              if (parse_at[1] == LBRACE)
                {
                  /* create subtokenizer */
                  tokenizer->istring_subtokenizer = dang_tokenizer_new (tokenizer->cp.filename);
                  tokenizer->istring_subtokenizer->cp.line = tokenizer->cp.line;
                  tokenizer->istring_subtokenizer->brace_terminated = 1;
                  parse_at += 2;
                  if (!dang_tokenizer_feed (tokenizer->istring_subtokenizer, parse_end - parse_at, parse_at, error))
                    {
                      //dang_warning ("failed feeding subtokenizer");
                      return FALSE;
                    }

                  dang_util_array_clear (&tokenizer->data);
                  DANG_UTIL_ARRAY_INIT (&tokenizer->data, char);
                  if (tokenizer->istring_subtokenizer->got_terminal_brace)
                    {
                      DangTokenInterpolatedPiece piece;
                      unsigned i;
                      piece.type = DANG_TOKEN_INTERPOLATED_PIECE_TOKENS;
                      dang_code_position_copy (&piece.code_position,
                                               &tokenizer->cp);
                      piece.info.tokens.n = tokenizer->istring_subtokenizer->n_tokens;
                      piece.info.tokens.array = dang_new (DangToken *, piece.info.tokens.n);
                      for (i = 0; i < piece.info.tokens.n; i++)
                        piece.info.tokens.array[i]
                          = dang_tokenizer_pop_token (tokenizer->istring_subtokenizer);
                      dang_util_array_append (&tokenizer->istring_pieces, 1, &piece);

                      /* steal raw data back from tokenizer */
                      {
                        DangUtilArray tmp = tokenizer->istring_subtokenizer->data;
                        tokenizer->istring_subtokenizer->data = tokenizer->data;
                        tokenizer->data = tmp;
                      }

                      parse_at = tokenizer->data.data;
                      parse_end = parse_at + tokenizer->data.len;

                      tokenizer->cp.line = tokenizer->istring_subtokenizer->cp.line;
                      dang_tokenizer_free (tokenizer->istring_subtokenizer);
                      tokenizer->istring_subtokenizer = NULL;
                    }
                }
              else if (isalpha (parse_at[1]) || parse_at[1] == '_')
                {
                  /* bareword expr */
                  char *end;
                  char *name;
                  DangTokenInterpolatedPiece piece;
                  parse_at++;
                  switch (parse_bareword (parse_at, parse_end, &end,
                                          &name, error))
                    {
                    case TOKENIZE_RESULT_SUCCESS:
                      token = dang_token_bareword_take (name);
                      dang_code_position_copy (&token->any.code_position,
                                               &tokenizer->cp);
                      piece.type = DANG_TOKEN_INTERPOLATED_PIECE_TOKENS;
                      dang_code_position_copy (&piece.code_position,
                                               &tokenizer->cp);
                      piece.info.tokens.n = 1;
                      piece.info.tokens.array = dang_new (DangToken*, 1);
                      piece.info.tokens.array[0] = token;
                      dang_util_array_append (&tokenizer->istring_pieces, 1, &piece);
                      parse_at = end;
                      break;
                    case TOKENIZE_RESULT_NEEDS_MORE_DATA:
                      goto done;
                    case TOKENIZE_RESULT_ERROR:
                      return FALSE;
                    }
                }
              else
                {
                  dang_set_error (error, "%s:%u: unexpected char '%c' after $",
                                  tokenizer->cp.filename->str,
                                  tokenizer->cp.line,
                                  parse_at[1]);
                  return FALSE;
                }
            }
          else if (*parse_at == '\\')
            {
              /* parse escape sequence */
              char *end;
              if (parse_at + 1 == parse_end)
                goto done;
              switch (parse_escape_seq (parse_at + 1, parse_end, &end,
                                        &tokenizer->istring_buf, error))
                {
                case TOKENIZE_RESULT_ERROR:
                  return FALSE;
                case TOKENIZE_RESULT_NEEDS_MORE_DATA:
                  goto done;
                case TOKENIZE_RESULT_SUCCESS:
                  parse_at = end;
                  break;
                }
            }
          else if (*parse_at == '"')
            {
              DangTokenInterpolatedPiece *pieces;
              maybe_clear_istring_buf (tokenizer);
              parse_at++;
              tokenizer->in_interpolated_string = 0;
              dang_util_array_clear (&tokenizer->istring_buf);

              /* Create INTERPOLATED_STRING token */

              pieces = dang_memdup (tokenizer->istring_pieces.data,
                                    tokenizer->istring_pieces.len * sizeof (DangTokenInterpolatedPiece));
              token = dang_token_interpolated_string_take (tokenizer->istring_pieces.len, pieces);

              push_token (tokenizer, token);

              dang_util_array_clear (&tokenizer->istring_pieces);
              break;
            }
          else
            {
              char *tmp = parse_at + 1;
              unsigned n_newlines = 0;
              while (tmp < parse_end
                     && *tmp != '$'
                     && *tmp != '\\'
                     && *tmp != '"')
                {
                  if (*tmp == '\n')
                    n_newlines++;
                  tmp++;
                }
              dang_util_array_append (&tokenizer->istring_buf,
                                 tmp - parse_at, parse_at);
              parse_at = tmp;
              tokenizer->cp.line += n_newlines;
            }
        }
    }
  else if (tokenizer->in_trailing_comment)
    {
      while (parse_at < parse_end && *parse_at != '\n')
        parse_at++;
      if (parse_at == parse_end)
        goto done;
      tokenizer->in_trailing_comment = 0;
    }
  else if (tokenizer->in_c_comment)
    {
      while (parse_at + 1 < parse_end
          && !(parse_at[0] == '*' && parse_at[1] == '/'))
        {
          if (*parse_at == '\n')
            tokenizer->cp.line += 1;
          parse_at++;
        }
      if (parse_at + 1 == parse_end)
        goto done;
      parse_at += 2;
      tokenizer->in_c_comment = 0;
    }
  else if (tokenizer->lit_tokenizer != NULL)
    {
      DangLiteralTokenizer *lit_tok = tokenizer->lit_tokenizer;
      void *lit_tok_state = tokenizer->lit_tokenizer_state;
      unsigned used_out;
      DangToken *token;
      switch (lit_tok->tokenize (lit_tok,
                                 lit_tok_state,
                                 parse_end - parse_at, 
                                 parse_at,
                                 &used_out,
                                 &token,
                                 error))
        {
        case DANG_LITERAL_TOKENIZER_DONE:
          /* push token */
          dang_code_position_copy (&token->any.code_position,
                                   &tokenizer->lit_tokenizer_cp);
          push_token (tokenizer, token);

          /* free state */
          if (lit_tok->destruct_state)
	    (*lit_tok->destruct_state) (lit_tok, lit_tok_state);
          dang_free (lit_tok_state);
          dang_code_position_clear (&tokenizer->lit_tokenizer_cp);
          tokenizer->lit_tokenizer = NULL;
          tokenizer->lit_tokenizer_state = NULL;

          /* advance parse_at */
          parse_at += used_out;
          if (parse_at == parse_end)
            goto done;

          goto restart;

        case DANG_LITERAL_TOKENIZER_CONTINUE:
          goto done;

        case DANG_LITERAL_TOKENIZER_ERROR:
          dang_error_add_suffix (*error,
                                 "(in \\%s began at " DANG_CP_FORMAT ")",
                                 tokenizer->lit_tokenizer->name,
                                 DANG_CP_ARGS (tokenizer->lit_tokenizer_cp));
          return FALSE;
        }
    }
  while (parse_at < parse_end && isspace (*parse_at))
    {
      if (*parse_at == '\n')
        tokenizer->cp.line += 1;
      parse_at++;
    }

  if (parse_at == parse_end)
    goto done;

  if (isdigit (*parse_at)
   || (parse_at + 1 < parse_end         /* -1 or -.1  */
        && (parse_at[0] == '-' 
            && (isdigit (parse_at[1]) || parse_at[1] == '.'))))
    {
      /* numeric literal */
      DangValueType *type;
      void *value;
      char *end_num;
      switch (parse_numeric_literal (parse_at, parse_end, &end_num,
                                     &type, &value, error))
        {
        case TOKENIZE_RESULT_ERROR:
          add_position_prefix (tokenizer, error);
          return FALSE;
        case TOKENIZE_RESULT_NEEDS_MORE_DATA:
          goto done;
        case TOKENIZE_RESULT_SUCCESS:
          token = dang_token_literal_take (type, value);
          push_token (tokenizer, token);
          parse_at = end_num;
          goto restart;
        }
    }
  else if (isalpha (*parse_at) || *parse_at == '_')
    {
      /* bareword */
      char *name;
      char *end_bareword;
      switch (parse_bareword (parse_at, parse_end, &end_bareword,
                              &name, error))
        {
        case TOKENIZE_RESULT_ERROR:
          add_position_prefix (tokenizer, error);
          return FALSE;
        case TOKENIZE_RESULT_NEEDS_MORE_DATA:
          goto done;
        case TOKENIZE_RESULT_SUCCESS:
          token = dang_token_bareword_take (name);
          push_token (tokenizer, token);
          parse_at = end_bareword;
          goto restart;
        }
    }
  else if (*parse_at == '\\')
    {
      char *at;
      char *name;
      DangLiteralTokenizer *lit_tokenizer;
      void *state;
      at = parse_at + 1;
      while (at < parse_end && (isalnum (*at) || *at == '_'))
        at++;
      if (at == parse_end)
        goto done;
      name = dang_strndup (parse_at + 1, at - (parse_at + 1));
      GSK_RBTREE_LOOKUP_COMPARATOR (GET_LITERAL_TOKENIZER_TREE (),
				    name,
				    COMPARE_STR_TO_LIT_TOKENIZER,
				    lit_tokenizer);
      dang_free (name);
      if (lit_tokenizer == NULL)
	{
	  dang_set_error (error, "%s:%u: unknown tokenizer magic %s",
			  tokenizer->cp.filename->str,
			  tokenizer->cp.line,
			  (char*) tokenizer->istring_buf.data);
	  return FALSE;
	}

      /* save literal-tokenizer */
      tokenizer->lit_tokenizer = lit_tokenizer;

      /* prepare literal-tokenizer state */
      state = dang_malloc0 (lit_tokenizer->sizeof_state);
      tokenizer->lit_tokenizer_state = state;
      if (lit_tokenizer->init_state != NULL)
	lit_tokenizer->init_state (lit_tokenizer, state);

      /* note the position that the literal token begins at */
      dang_code_position_copy (&tokenizer->lit_tokenizer_cp, &tokenizer->cp);
 
      /* skip past magic */
      parse_at = at;

      /* resume tokenizing (XXX: could just goto the appropriate place!) */
      goto restart;
    }
  else if (ispunct (*parse_at) && *parse_at != '"' && *parse_at != '\'')
    {
      /* operator */
      char *op_str;
      char *end_op;
      if (*parse_at == LBRACE || *parse_at == RBRACE)
        {
          if (tokenizer->brace_terminated
              && tokenizer->brace_balance == 0
              && *parse_at == RBRACE)
            {
              parse_at++;               /* swallow the right brace! */
              tokenizer->got_terminal_brace = 1;
              goto done;
            }
          if (*parse_at == LBRACE)
            tokenizer->brace_balance++;
          else if (tokenizer->brace_balance == 0)
            {
              dang_set_error (error,
                              "%s:%u: unbalanced right-brace encountered",
                              tokenizer->cp.filename->str, tokenizer->cp.line);
              return FALSE;
            }
          else
            tokenizer->brace_balance--;
          token = dang_token_operator_take (dang_strndup (parse_at, 1));
          push_token (tokenizer, token);
          parse_at++;
        }
      else
        {
          if (parse_at + 1 < parse_end && parse_at[0] == '/')
            {
              if (parse_at[1] == '*')
                {
                  tokenizer->in_c_comment = 1;
                  parse_at += 2;
                  goto restart;
                }
              else if (parse_at[1] == '/')
                {
                  tokenizer->in_trailing_comment = 1;
                  parse_at += 2;
                  goto restart;
                }
            }
          switch (parse_operator (parse_at, parse_end, &end_op,
                                  &op_str, error))
            {
            case TOKENIZE_RESULT_ERROR:
              add_position_prefix (tokenizer, error);
              return FALSE;
            case TOKENIZE_RESULT_NEEDS_MORE_DATA:
              goto done;
            case TOKENIZE_RESULT_SUCCESS:
              token = dang_token_operator_take (op_str);
              push_token (tokenizer, token);
              parse_at = end_op;
              goto restart;
            }
        }
    }
  else if (*parse_at == '"')
    {
      tokenizer->in_interpolated_string = 1;
      DANG_UTIL_ARRAY_INIT (&tokenizer->istring_buf, char);
      DANG_UTIL_ARRAY_INIT (&tokenizer->istring_pieces, DangTokenInterpolatedPiece);
      parse_at++;
      goto restart;
    }
  else if (*parse_at == '\'')
    {
      dang_unichar c;
      char *end_char_lit;
      switch (parse_char_literal (parse_at, parse_end, &end_char_lit,
                                  &c, error))
        {
        case TOKENIZE_RESULT_ERROR:
          add_position_prefix (tokenizer, error);
          return FALSE;
        case TOKENIZE_RESULT_NEEDS_MORE_DATA:
          goto done;
        case TOKENIZE_RESULT_SUCCESS:
          token = dang_token_literal_take (dang_value_type_char (),
                                           dang_memdup (&c, sizeof(c)));
          push_token (tokenizer, token);
          parse_at = end_char_lit;
          goto restart;
        }
    }
  else
    {
      dang_set_error (error,
                      "%s:%u: unexpected char 0x%02x",
                      tokenizer->cp.filename->str,
                      tokenizer->cp.line,
                      (unsigned char) *parse_at);
      return FALSE;
    }
  if (parse_at < parse_end)
    goto restart;
done:
  dang_util_array_remove (&tokenizer->data, 0, parse_at - (char*)(tokenizer->data.data));
  return TRUE;
}

DangToken     *dang_tokenizer_pop_token (DangTokenizer  *tokenizer)
{
  DangToken *rv;
  if (tokenizer->n_tokens == 0)
    return NULL;
  rv = tokenizer->token_queue[tokenizer->first_token++];
  tokenizer->n_tokens--;
  if (tokenizer->first_token == tokenizer->queue_size)
    tokenizer->first_token = 0;
  return rv;
}

void           dang_tokenizer_free      (DangTokenizer  *tokenizer)
{
  DangToken *tok;
  while ((tok=dang_tokenizer_pop_token (tokenizer)) != NULL)
    dang_token_unref (tok);
  dang_free (tokenizer->token_queue);
  dang_util_array_clear (&tokenizer->data);
  dang_code_position_clear (&tokenizer->cp);
  if (tokenizer->lit_tokenizer != NULL)
    {
      dang_code_position_clear (&tokenizer->lit_tokenizer_cp);
      if (tokenizer->lit_tokenizer->destruct_state)
        tokenizer->lit_tokenizer->destruct_state (tokenizer->lit_tokenizer,
                                                  tokenizer->lit_tokenizer_state);
      dang_free (tokenizer->lit_tokenizer_state);
    }
  dang_free (tokenizer);
}

void dang_literal_tokenizer_register  (DangLiteralTokenizer *lit_tokenizer)
{
  DangLiteralTokenizer *conflict;
  GSK_RBTREE_INSERT (GET_LITERAL_TOKENIZER_TREE (), lit_tokenizer, conflict);
  dang_assert (conflict == NULL);
}
