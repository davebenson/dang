
typedef struct _DangTokenizer DangTokenizer;
DangTokenizer *dang_tokenizer_new       (DangString     *filename);
dang_boolean   dang_tokenizer_feed      (DangTokenizer  *tokenizer,
                                         unsigned        len,
                                         char           *str,
                                         DangError     **error);
DangToken     *dang_tokenizer_pop_token (DangTokenizer  *tokenizer);
void           dang_tokenizer_free      (DangTokenizer  *tokenizer);
