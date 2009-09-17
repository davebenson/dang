#include "dang.h"

#include "gsklistmacros.h"
static DangToken *first,*last;
#define GET_TOKEN_LIST() DangToken*,first,last,any.prev,any.next

const char *dang_token_type_name (DangTokenType type)
{
  switch (type)
    {
    case DANG_TOKEN_TYPE_LITERAL: return "literal";
    case DANG_TOKEN_TYPE_BAREWORD: return "bareword";
    case DANG_TOKEN_TYPE_INTERPOLATED_STRING: return "interpolated_string";
    case DANG_TOKEN_TYPE_OPERATOR: return "operator";
    default: return "*bad token-type*";
    }
}
char *dang_token_make_string (DangToken *token,
                              dang_boolean include_pos)
{
  if (include_pos && token->any.code_position.filename != NULL)
    {
      char *subrv = dang_token_make_string (token, FALSE);
      char *rv = dang_strdup_printf ("%s [at %s:%u]",
                                     subrv,
                                     token->any.code_position.filename->str,
                                     token->any.code_position.line);
      dang_free (subrv);
      return rv;
    }
  switch (token->type)
    {
    case DANG_TOKEN_TYPE_OPERATOR:
      return dang_strdup_printf ("operator: %s", token->v_operator.str);
    case DANG_TOKEN_TYPE_BAREWORD:
      return dang_strdup_printf ("bareword: %s", token->v_bareword.name);
    case DANG_TOKEN_TYPE_INTERPOLATED_STRING:
      {
        return dang_strdup_printf ("interpolated_string: %u pieces",
                                   token->v_interpolated_string.n_pieces);
      }
    case DANG_TOKEN_TYPE_LITERAL:
      {
        char *val = dang_value_to_string (token->v_literal.type, token->v_literal.value);
        char *rv = dang_strdup_printf ("literal: %s: %s",
                                       token->v_literal.type->full_name, val);
        dang_free (val);
        return rv;
      }
    }
  return NULL;
}

DangToken *dang_token_ref   (DangToken *token)
{
  ++token->any.ref_count;
  return token;
}

void       dang_token_unref (DangToken *token)
{
  if (--(token->any.ref_count) == 0)
    {
      unsigned i, j;
      GSK_LIST_REMOVE (GET_TOKEN_LIST(),token);
      switch (token->type)
        {
        case DANG_TOKEN_TYPE_OPERATOR:
          dang_free (token->v_operator.str);
          break;
        case DANG_TOKEN_TYPE_BAREWORD:
          dang_free (token->v_bareword.name);
          break;
        case DANG_TOKEN_TYPE_INTERPOLATED_STRING:
          for (i = 0; i < token->v_interpolated_string.n_pieces; i++)
            {
              DangTokenInterpolatedPiece *p;
              p = token->v_interpolated_string.pieces + i;
              switch (p->type)
                {
                case DANG_TOKEN_INTERPOLATED_PIECE_TOKENS:
                  for (j = 0; j < p->info.tokens.n; j++)
                    dang_token_unref (p->info.tokens.array[j]);
                  dang_free (p->info.tokens.array);
                  break;
                case DANG_TOKEN_INTERPOLATED_PIECE_STRING:
                  dang_free (p->info.string);
                  break;
                }
              dang_code_position_clear (&p->code_position);
            }
          dang_free (token->v_interpolated_string.pieces);
          break;

        case DANG_TOKEN_TYPE_LITERAL:
          if (token->v_literal.type->destruct)
            token->v_literal.type->destruct (token->v_literal.type, token->v_literal.value);
          dang_free (token->v_literal.value);
          break;
        }
      dang_code_position_clear (&token->any.code_position);
      dang_free (token);
    }
}


static DangToken *
raw_alloc (DangTokenType type)
{
  DangToken *rv = dang_new (DangToken, 1);
  rv->any.type = type;
  rv->any.ref_count = 1;
  dang_code_position_init (&rv->any.code_position);
  GSK_LIST_APPEND (GET_TOKEN_LIST (), rv);
  return rv;
}

DangToken *
dang_token_literal_take (DangValueType *type,
                       void          *value)
{
  DangToken *rv = raw_alloc (DANG_TOKEN_TYPE_LITERAL);
  rv->v_literal.type = type;
  rv->v_literal.value = value;
  return rv;
}

DangToken *
dang_token_bareword_take (char     *name)
{
  DangToken *rv = raw_alloc (DANG_TOKEN_TYPE_BAREWORD);
  rv->v_bareword.name = name;
  return rv;
}

DangToken *dang_token_operator_take (char *str)
{
  DangToken *rv = raw_alloc (DANG_TOKEN_TYPE_OPERATOR);
  rv->v_operator.str = str;
  return rv;
}

DangToken *
dang_token_interpolated_string_take (unsigned n_pieces,
                                     DangTokenInterpolatedPiece* pieces)
{
  DangToken *rv;
  rv = raw_alloc (DANG_TOKEN_TYPE_INTERPOLATED_STRING);
  rv->v_interpolated_string.n_pieces = n_pieces;
  rv->v_interpolated_string.pieces = pieces;
  if (n_pieces == 1 && pieces[0].type == DANG_TOKEN_INTERPOLATED_PIECE_STRING)
    {
      DangString **val = dang_new (DangString *, 1);
      DangToken *rv2;
      *val = dang_string_new (pieces[0].info.string);
      rv2 = dang_token_literal_take (dang_value_type_string(), val);
      dang_token_unref (rv);
      rv = rv2;
    }
  return rv;
}

void _dang_tokens_dump_all (void)
{
  DangToken *at;
  for (at = first; at; at = at->any.next)
    {
      switch (at->type)
        {
        case DANG_TOKEN_TYPE_BAREWORD:
          dang_warning ("leaked token: token %p: bareword: %s", at, at->v_bareword.name);
          break;
        case DANG_TOKEN_TYPE_OPERATOR:
          dang_warning ("leaked token: token %p: operator: %s", at, at->v_operator.str);
          break;
        default:
          dang_warning ("leaked token: token %p: %s", at, dang_token_type_name (at->type));
          break;
        }
    }
}
