
/* Parse text into tokens */
typedef struct _DangTokenizer DangTokenizer;
DangTokenizer *dang_tokenizer_new       (DangString     *filename);
dang_boolean   dang_tokenizer_feed      (DangTokenizer  *tokenizer,
                                         unsigned        len,
                                         char           *str,
                                         DangError     **error);
DangToken     *dang_tokenizer_pop_token (DangTokenizer  *tokenizer);
void           dang_tokenizer_free      (DangTokenizer  *tokenizer);


/* magic token registration */
typedef enum
{
  DANG_LITERAL_TOKENIZER_DONE,
  DANG_LITERAL_TOKENIZER_CONTINUE,
  DANG_LITERAL_TOKENIZER_ERROR
} DangTokenizerResult;

typedef struct _DangLiteralTokenizer DangLiteralTokenizer;
struct _DangLiteralTokenizer
{
  const char *name;
  unsigned sizeof_state;
  void              (*init_state) (DangLiteralTokenizer *lit_tokenizer,
                                   void                 *state);
  /* If this returns DONE, then *text_used_out is set to the number of
     bytes of data used and *token_out is the returned token.
     If it returns ERROR, then *error will be set.
     If it returns CONTINUE, then more data is required. */
  DangTokenizerResult (*tokenize) (DangLiteralTokenizer *lit_tokenizer,
                                   void                 *state,
                                   unsigned              len,
                                   const char           *text,
                                   unsigned             *text_used_out,
                                   DangToken           **token_out,
                                   DangError           **error_out);

  void          (*destruct_state) (DangLiteralTokenizer *lit_tokenizer,
                                   void                 *state);

  /* reserved for internal use */
  DangLiteralTokenizer *lt_parent, *lt_left, *lt_right;
  dang_boolean is_red;
};

#define DANG_LITERAL_TOKENIZER_INTERNALS_INIT NULL, NULL, NULL, FALSE


void dang_literal_tokenizer_register  (DangLiteralTokenizer *lit_tokenizer);

