#include <string.h>
#include <stdlib.h>
#include "dsk.h"

DskJsonParser *
dsk_json_parser_new (void)
{
  DskJsonParser *parser = g_new (DskJsonParser, 1);
  parser->lex_state = JSON_LEX_STATE_INIT;
  parser->str = g_string_new ("");
  parser->parser = DskJsonParseAlloc (g_malloc);
  parser->xml_nodes = g_queue_new ();
  parser->static_syntax_error = NULL;
  parser->line_no = 1;
  return parser;
}

static gboolean
add_token                (DskJsonParser *parser,
                          int           yymajor,
                          char         *str,
                          DskError      **error)
{
  DskJsonParse (parser->parser, yymajor, str, parser);
  if (parser->static_syntax_error)
    {
      dsk_set_error (error,
                   "syntax error: %s (line %u)",
                   parser->static_syntax_error, parser->line_no);
      return FALSE;
    }
  return TRUE;
}

static gboolean
add_token_copying_str (DskJsonParser *parser,
                       int           yymajor,
                       DskError      **error)
{
  char *copy = g_malloc (parser->str->len + 1);
  memcpy (copy, parser->str->str, parser->str->len);
  copy[parser->str->len] = 0;
  return add_token (parser, yymajor, copy, error);
}

gboolean
dsk_json_parser_feed(DskJsonParser *parser,
                    unsigned      n_bytes,
                    const guint8 *bytes,
                    DskError      **error)
{
  while (n_bytes > 0)
    {
      switch (parser->lex_state)
        {
        case JSON_LEX_STATE_INIT:
          while (n_bytes > 0 && g_ascii_isspace (*bytes))
            {
              bytes++;
              n_bytes--;
            }
          if (n_bytes == 0)
            break;
          switch (*bytes)
            {
            case 't': case 'T':
              parser->lex_state = JSON_LEX_STATE_TRUE;
              parser->fixed_n_chars = 1;
              bytes++;
              n_bytes--;
              break;
            case 'f': case 'F':
              parser->lex_state = JSON_LEX_STATE_FALSE;
              parser->fixed_n_chars = 1;
              bytes++;
              n_bytes--;
              break;
            case 'n': case 'N':
              parser->lex_state = JSON_LEX_STATE_NULL;
              parser->fixed_n_chars = 1;
              bytes++;
              n_bytes--;
              break;
            case '"':
              parser->lex_state = JSON_LEX_STATE_IN_DQ;
              g_string_set_size (parser->str, 0);
              bytes++;
              n_bytes--;
              break;
            case '-': case '+':
            case '0': case '1': case '2': case '3': case '4': 
            case '5': case '6': case '7': case '8': case '9': 
              parser->lex_state = JSON_LEX_STATE_IN_NUMBER;
              g_string_set_size (parser->str, 1);
              parser->str->str[0] = *bytes++;
              n_bytes--;
              break;

#define WRITE_CHAR_TOKEN_CASE(character, SHORTNAME) \
            case character: \
              if (!add_token (parser, UF_JSON_TOKEN_##SHORTNAME, NULL, error)) \
                return FALSE; \
              n_bytes--; \
              bytes++; \
              break
            WRITE_CHAR_TOKEN_CASE('{', LBRACE);
            WRITE_CHAR_TOKEN_CASE('}', RBRACE);
            WRITE_CHAR_TOKEN_CASE('[', LBRACKET);
            WRITE_CHAR_TOKEN_CASE(']', RBRACKET);
            WRITE_CHAR_TOKEN_CASE(',', COMMA);
            WRITE_CHAR_TOKEN_CASE(':', COLON);
#undef WRITE_CHAR_TOKEN_CASE

            case '\n':
              parser->line_no++;
              n_bytes--;
              bytes++;
              break;
            case '\t': case '\r': case ' ':
              n_bytes--;
              bytes++;
              break;
            default:
              dsk_set_error (error,
                           "unexpected character '%c' (0x%02x) in json (line %u)",
                           *bytes, *bytes, parser->line_no);
              return FALSE;
            }
          break;

#define WRITE_FIXED_BAREWORD_CASE(SHORTNAME, lc, UC, length) \
        case JSON_LEX_STATE_##SHORTNAME: \
          if (parser->fixed_n_chars == length) \
            { \
              /* are we at end of string? */ \
              if (g_ascii_isalnum (*bytes)) \
                { \
                  dsk_set_error (error, \
                               "got %c after '%s' (line %u)", *bytes, lc, \
                               parser->line_no); \
                  return FALSE; \
                } \
              else \
                { \
                  parser->lex_state = JSON_LEX_STATE_INIT; \
                  if (!add_token (parser, UF_JSON_TOKEN_##SHORTNAME, \
                                  g_strdup (lc), error)) \
                    return FALSE; \
                } \
            } \
          else if (*bytes == lc[parser->fixed_n_chars] \
                || *bytes == UC[parser->fixed_n_chars]) \
            { \
              parser->fixed_n_chars += 1; \
              n_bytes--; \
              bytes++; \
            } \
          else \
            { \
              dsk_set_error (error, \
                           "unexpected character '%c' (parsing %s) (line %u)", \
                           *bytes, UC, parser->line_no); \
              return FALSE; \
            } \
          break;
        WRITE_FIXED_BAREWORD_CASE(TRUE, "true", "TRUE", 4);
        WRITE_FIXED_BAREWORD_CASE(FALSE, "false", "FALSE", 5);
        WRITE_FIXED_BAREWORD_CASE(NULL, "null", "NULL", 4);
#undef WRITE_FIXED_BAREWORD_CASE

        case JSON_LEX_STATE_IN_DQ:
          if (*bytes == '"')
            {
              if (!add_token_copying_str (parser, UF_JSON_TOKEN_STRING, error))
                return FALSE;
              bytes++;
              n_bytes--;
              parser->lex_state = JSON_LEX_STATE_INIT;
            }
          else if (*bytes == '\\')
            {
              n_bytes--;
              bytes++;
              parser->bs_sequence_len = 0;
              parser->lex_state = JSON_LEX_STATE_IN_DQ_BS;
            }
          else
            {
              unsigned i;
              if (*bytes == '\n')
                parser->line_no++;
              for (i = 1; i < n_bytes; i++)
                if (bytes[i] == '"' || bytes[i] == '\\')
                  break;
                else if (bytes[i] == '\n')
                  parser->line_no++;
              g_string_append_len (parser->str, (char*)bytes, i);
              n_bytes -= i;
              bytes += i;
            }
          break;
        case JSON_LEX_STATE_IN_DQ_BS:
          if (parser->bs_sequence_len == 0)
            {
              switch (*bytes)
                {
#define WRITE_BS_CHAR_CASE(bschar, cchar) \
                case bschar: \
                  g_string_append_c (parser->str, cchar); \
                  bytes++; \
                  n_bytes--; \
                  parser->lex_state = JSON_LEX_STATE_IN_DQ; \
                  break
                WRITE_BS_CHAR_CASE('b', '\b');
                WRITE_BS_CHAR_CASE('f', '\f');
                WRITE_BS_CHAR_CASE('n', '\n');
                WRITE_BS_CHAR_CASE('r', '\r');
                WRITE_BS_CHAR_CASE('t', '\t');
                WRITE_BS_CHAR_CASE('/', '/');
                WRITE_BS_CHAR_CASE('\\', '\\');
#undef WRITE_BS_CHAR_CASE
                case 'u':
                  parser->bs_sequence[parser->bs_sequence_len++] = *bytes++;
                  n_bytes--;
                  break;
                default:
                  dsk_set_error (error,
                                 "invalid character '%c' after '\\' (line %u)",
                                 *bytes, parser->line_no);
                  return FALSE;
                }
            }
          else
            {
              /* must be \uxxxx */
              if (!g_ascii_isxdigit (*bytes))
                {
                  dsk_set_error (error,
                                 "expected 4 hex digits after \\u, got %s (line %u)",
                                 dsk_ascii_byte_name (*bytes), parser->line_no);
                  return FALSE;
                }
              parser->bs_sequence[parser->bs_sequence_len++] = *bytes++;
              n_bytes--;
              if (parser->bs_sequence_len == 5)
                {
                  char utf8bdsk[8];
                  guint value;
                  parser->bs_sequence[5] = 0;
                  value = strtoul (parser->bs_sequence + 1, NULL, 16);
                  g_string_append_len (parser->str,
                                       utf8bdsk,
                                       g_unichar_to_utf8 (value, utf8bdsk));
                  parser->lex_state = JSON_LEX_STATE_IN_DQ;
                }
            }
          break;
        case JSON_LEX_STATE_IN_NUMBER:
          if (g_ascii_isdigit (*bytes)
           || *bytes == '.'
           || *bytes == 'e'
           || *bytes == 'E'
           || *bytes == '+'
           || *bytes == '-')
            {
              g_string_append_c (parser->str, *bytes++);
              n_bytes--;
            }
          else
            {
              /* append the number token */
              if (!add_token_copying_str (parser, UF_JSON_TOKEN_NUMBER, error))
                return FALSE;

              /* go back to init state (do not consume character) */
              parser->lex_state = JSON_LEX_STATE_INIT;
            }
          break;
        default:
          g_error ("unhandled lex state %u", parser->lex_state);
        }
    }
  return TRUE;
}

gboolean
dsk_json_parser_end_parse (DskJsonParser *parser,
                          DskError      **error)
{
  return add_token (parser, 0, NULL, error);
}

DskXml *
dsk_json_parser_pop (DskJsonParser *parser)
{
  return g_queue_pop_head (parser->xml_nodes);
}

void
dsk_json_parser_free(DskJsonParser *parser)
{
  g_list_foreach (parser->xml_nodes->head, (GFunc) dsk_xml_unref, NULL);
  g_queue_free (parser->xml_nodes);
  DskJsonParseFree (parser->parser, g_free);
  g_string_free (parser->str, TRUE);
  g_free (parser);
}
