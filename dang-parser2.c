#include "dang.h"
#include "dang-parser2.h"

void dang_parser_nonterminal_add_pattern     (DangParserNonterminal nt,
                                              unsigned              n_pieces,
                                              DangPatternPiece     *pieces);


/* public api */
DangParser    *dang_parser_new (void);
dang_boolean   dang_parser_add_token         (DangParser           *parser,
                                              DangToken            *token);
void           dang_parser_parse_nonterminal (DangParser           *parser,
                                              DangParserNonterminal nt,
                                              DangParserResult     *result);


/* --- Standard Nonterminals --- */
static void
handle_parse_expr (unsigned          n_trees,
                   DangTokenTree    *trees,
                   dang_boolean      terminated;
                   void             *parser_data,
                   DangParserResult *result)
{
  dang_boolean permit_void = (dang_boolean) parser_data;

  for (i = 0; i < n_trees; i++)
    {
      if (trees[i].type == DANG_TOKEN_TREE_TYPE_TOKEN
       && trees[i].info.token->type == DANG_TOKEN_TYPE_OPERATOR
       && is_expression_terminator (trees[i].info.token->v_operator.str))
        break;
    }
  if (i == n_trees && !terminated)
    {
      result->type = DANG_PARSER_RESULT_NEED_MORE_DATA;
      return;
    }

  if (i == 0)
    {
      if (permit_void)
        {
          result->type = DANG_PARSER_RESULT_SUCCESS;
          result->trees_used = 0;
          result->expr = dang_expr_new_bareword ("$void");
          if (n_trees > 0)
            expr_set_cp (result->expr, &trees[0].code_position);
        }
      else
        {
          result->type = DANG_PARSER_RESULT_ERROR;
          result->info.error = dang_error_new ("empty expression not allowed here");
          if (n_trees > 0)
            dang_error_add_pos_suffix (result->info.error, &trees[0].code_position);
        }
      return;
    }


  /* clamp array at the end of expression */
  n_trees = i;


  /* Next

  DANG_TOKEN_TREE_TYPE_TOKEN
  ...
}

/* --- Standard Constructs --- */
static DangPatternPiece for_loop_subpieces[] =
{
  { DANG_PATTERN_PIECE_NONTERMINAL, NULL, DANG_PARSER_NONTERMINAL_OPT_EXPR, 0, 0, NULL },
  { DANG_PATTERN_PIECE_OP, ";", 0, 0, 0, NULL },
  { DANG_PATTERN_PIECE_NONTERMINAL, NULL, DANG_PARSER_NONTERMINAL_OPT_EXPR, 0, 0, NULL },
  { DANG_PATTERN_PIECE_OP, ";", 0, 0, 0, NULL },
  { DANG_PATTERN_PIECE_NONTERMINAL, NULL, DANG_PARSER_NONTERMINAL_OPT_EXPR, 0, 0, NULL },
};

static DangPatternPiece for_loop_pieces[] =
{
  { DANG_PATTERN_PIECE_SPECIFIC_BAREWORD, "for", 0, 0, 0, NULL },
  { DANG_PATTERN_PIECE_BRACKETED, NULL, 0, DANG_TOKEN_TREE_PAREN,
    DANG_N_ELEMENTS (for_loop_subpieces), for_loop_subpieces },
  { DANG_PATTERN_PIECE_NONTERMINAL, NULL, DANG_PARSER_NONTERMINAL_STATEMENT,
    0, 0, NULL }
};


void _dang_parser_init (void)
{
  DangParserNonterminal nt;
  DangPatternPiece pieces[10];

  nt = dang_parser_nonterminal_register ("expr");
  dang_assert (nt == DANG_PARSER_NONTERMINAL_EXPR);
  nt = dang_parser_nonterminal_register ("statement");
  dang_assert (nt == DANG_PARSER_NONTERMINAL_STATEMENT);
  nt = dang_parser_nonterminal_register ("toplevel");
  dang_assert (nt == DANG_PARSER_NONTERMINAL_TOPLEVEL);
  nt = dang_parser_nonterminal_register ("opt_expr");
  dang_assert (nt == DANG_PARSER_NONTERMINAL_OPT_EXPR);

  /* register expr, opt_expr */
  dang_parser_nonterminal_set_func (DANG_PARSER_NONTERMINAL_EXPR,
                                    handle_parse_expr,
                                    (void*) FALSE,
                                    NULL);
  dang_parser_nonterminal_set_func (DANG_PARSER_NONTERMINAL_EXPR,
                                    handle_parse_expr,
                                    (void*) TRUE,
                                    NULL);


  dang_parser_nonterminal_add_pattern (DANG_PARSER_NONTERMINAL_STATEMENT,
                                       "$for",
                                       DANG_N_ELEMENTS (for_loop_pieces)
                                       for_loop_pieces);
}
