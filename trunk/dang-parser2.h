
typedef struct _DangTokenTree DangTokenTree;

typedef enum
{
  DANG_TOKEN_TREE_TYPE_SUBTREE,
  DANG_TOKEN_TREE_TYPE_TOKEN
} DangTokenTreeType;

typedef enum
{
  DANG_TOKEN_TREE_ANGLE_BRACKET,
  DANG_TOKEN_TREE_SQUARE_BRACKET,
  DANG_TOKEN_TREE_PAREN
} DangTokenTreeBracket;

struct _DangTokenTree
{
  DangTokenTreeType type;
  union {
    struct {
      DangTokenTreeBracket bracket;
      unsigned n_subnodes;
      DangTokenTree *subnodes;
    } subtree;
    DangToken *token;
  } info;
};


struct _DangParser
{
  DangUtilArray token_trees;
};

typedef enum
{
  DANG_PARSER_RESULT_SUCCESS,
  DANG_PARSER_RESULT_ERROR,
  DANG_PARSER_RESULT_NEED_MORE_DATA
} DangParserResultType;

typedef struct
{
  DangParserResultType type;
  union
  {
    DangError *error;
    struct {
      unsigned trees_used;
      DangExpr *expr;
    } success;
  } info;
} DangParserResult;

// parser: goes from an array of token-trees to a DangExpr
typedef void (*DangParserFunc)(unsigned          n_trees,
                               DangTokenTree    *trees,
                               dang_boolean      terminated;
                               void             *parser_data,
                               DangParserResult *result);

typedef enum
{
  DANG_PARSER_NONTERMINAL_EXPR,
  DANG_PARSER_NONTERMINAL_STATEMENT,
  DANG_PARSER_NONTERMINAL_TOPLEVEL,
  DANG_PARSER_NONTERMINAL_OPT_EXPR

  _DANG_PARSER_NONTERMINAL_IS4BYTES = (1<<16)

} DangParserNonterminal;                /* extensible */

DangParserNonterminal dang_parser_nonterminal_register (const char *name);

typedef enum
{
  DANG_PATTERN_PIECE_OP,
  DANG_PATTERN_PIECE_ANY_BAREWORD,
  DANG_PATTERN_PIECE_SPECIFIC_BAREWORD,
  DANG_PATTERN_PIECE_NONTERMINAL
  DANG_PATTERN_PIECE_REPEATED,
  DANG_PATTERN_PIECE_BRACKETED
} DangPatternPieceType;

#if 0
typedef struct _DangPatternPiece DangPatternPiece;
struct _DangPatternPiece
{
  DangPatternPieceType type;
  char *str;
  DangParserNonterminal nt;
  DangTokenTreeBracket bracket;
  unsigned n_subpieces;
  DangPatternPiece *subpieces;
};

void dang_parser_nonterminal_add_pattern     (DangParserNonterminal nt,
                                              unsigned              n_pieces,
                                              DangPatternPiece     *pieces);
void dang_parser_nonterminal_set_func        (DangParserNonterminal nt,
                                              DangParserFunc        func,
                                              void                 *func_data,
                                              DangDestroyNotify     destroy);
#endif


/* public api */
DangParser    *dang_parser_new (void);
dang_boolean   dang_parser_add_token         (DangParser           *parser,
                                              DangToken            *token);
void           dang_parser_parse_nonterminal (DangParser           *parser,
                                              DangParserNonterminal nt,
                                              DangParserResult     *result);

