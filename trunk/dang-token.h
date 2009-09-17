/* the tokenizer */
typedef struct _DangTokenAny DangTokenAny;
typedef struct _DangTokenOperator DangTokenOperator;
typedef struct _DangTokenBareword DangTokenBareword;
typedef struct _DangTokenInterpolatedString DangTokenInterpolatedString;
typedef struct _DangTokenLiteral DangTokenLiteral;
typedef union _DangToken DangToken;
typedef struct _DangTokenInterpolatedPiece DangTokenInterpolatedPiece;

typedef enum
{
  DANG_TOKEN_TYPE_OPERATOR,
  DANG_TOKEN_TYPE_BAREWORD,
  DANG_TOKEN_TYPE_INTERPOLATED_STRING,
  DANG_TOKEN_TYPE_LITERAL
} DangTokenType;
const char *dang_token_type_name (DangTokenType);

struct _DangTokenAny
{
  DangTokenType type;
  unsigned ref_count;
  DangCodePosition code_position;

  DangToken *prev,*next;
};

struct _DangTokenOperator
{
  DangTokenAny base;
  char *str;
};

struct _DangTokenBareword
{
  DangTokenAny base;
  char *name;
};
struct _DangTokenInterpolatedString
{
  DangTokenAny base;
  unsigned n_pieces;
  DangTokenInterpolatedPiece *pieces;
};

struct _DangTokenLiteral
{
  DangTokenAny base;
  DangValueType *type;
  void *value;
};

union _DangToken
{
  DangTokenType type;
  DangTokenAny any;
  DangTokenOperator v_operator;
  DangTokenBareword v_bareword;
  DangTokenInterpolatedString v_interpolated_string;
  DangTokenLiteral v_literal;
};

#if 0
DangToken *dang_tokenize_file  (const char *filename,
                                unsigned   *n_tokens_out,
                                DangError **error);
DangToken *dang_tokenize_string(DangString *report_filename,
                                const char *str,
                                unsigned   *n_tokens_out,
                                DangError **error);
#endif

DangToken *dang_token_ref   (DangToken *token);
void       dang_token_unref (DangToken *token);

/* for debugging */
char *dang_token_make_string (DangToken *token,
                              dang_boolean include_pos);


/* for interpolated strings */

typedef enum
{
  DANG_TOKEN_INTERPOLATED_PIECE_STRING,
  DANG_TOKEN_INTERPOLATED_PIECE_TOKENS
} DangTokenInterpolatedPieceType;

struct _DangTokenInterpolatedPiece
{
  DangTokenInterpolatedPieceType type;
  DangCodePosition code_position;
  union {
    char *string;
    struct {
      unsigned n;
      DangToken **array;
    } tokens;
  } info;
};

DangToken *dang_token_literal_take (DangValueType *type,
                                  void *value);
DangToken *dang_token_bareword_take (char *name);
DangToken *dang_token_operator_take (char *str);
DangToken *dang_token_interpolated_string_take (unsigned n_pieces,
                                                DangTokenInterpolatedPiece*);
