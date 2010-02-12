#include "dang.h"
#include "dang-parser2.h"

typedef enum
{
  DANG_PARSER_RESULT_NEED_MORE_DATA,
  DANG_PARSER_RESULT_EXPR,
  DANG_PARSER_RESULT_ERROR
} DangParserResultType;

typedef struct _DangParserResult DangParserResult;
struct _DangParserResult
{
  DangParserResultType type;
  union
  {
    DangExpr *expr;
    DangError *error;
  } info;
};

DangParser *
dang_parser_new (void)
{
  DangParser *rv = dang_new (DangParser, 1);
  DANG_UTIL_ARRAY_INIT (&rv->token_trees, DangTokenTree);
  return rv;
}

dang_boolean
dang_parser_add_token         (DangParser           *parser,
                               DangToken            *token)
{
  ...
}

void
dang_parser_parse_nonterminal (DangParser           *parser,
                               DangParserNonterminal nt,
                               DangParserResult     *result)
{
  ...
}


/* --- Standard Constructs --- */

void _dang_parser_init (void)
{
}
