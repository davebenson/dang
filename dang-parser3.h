
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


DangParser    *dang_parser_new (void);
dang_boolean   dang_parser_add_token         (DangParser           *parser,
                                              DangToken            *token);

