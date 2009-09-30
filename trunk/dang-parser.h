typedef struct _DangParser DangParser;
typedef struct _DangParserFactory DangParserFactory;
typedef struct _DangParseOptions DangParseOptions;

typedef enum
{
  DANG_PARSE_MODE_TOPLEVEL,
  DANG_PARSE_MODE_EXPR,
  DANG_PARSE_MODE_TYPE
} DangParseMode;

struct _DangParseOptions
{
  const char *factory_name;
  DangImports *imports;
  DangParseMode mode;
};

struct _DangParser
{
  dang_boolean     (*parse)     (DangParser *parser,
                                 DangToken  *token,
                                 DangError **error);
  dang_boolean     (*end_parse) (DangParser *parser,
                                 DangError **error);
  void             (*destroy)   (DangParser *parser);

  /* a queue of expressions. note: we may change this impl someday */
  DangUtilArray results;

  DangImports *imports;
};

#define dang_parser_peek_imports(parser)  ((parser)->imports)
void dang_parser_set_imports (DangParser *parser,
                              DangImports *imports);

struct _DangParserFactory
{
  DangParser *(*create_parser) (DangParserFactory *factory,
                                DangParseOptions  *options,
                                DangError        **error);
};
#define DANG_PARSE_OPTIONS_INIT                 \
        {                                       \
          "default",   /* factory_name */       \
          NULL,        /* imports */            \
          DANG_PARSE_MODE_TOPLEVEL              \
        }

DangParser     *dang_parser_create (DangParseOptions *options,
                                    DangError       **error);
dang_boolean    dang_parser_parse (DangParser *parser,
                                   DangToken  *token,
                                   DangError **error);
dang_boolean    dang_parser_end_parse (DangParser *parser,
                                   DangError **error);
DangExpr       *dang_parser_pop_expr(DangParser *parser);
void            dang_parser_destroy(DangParser *parser);

unsigned dang_parser_get_n_results (DangParser *);

/* --- protected --- */
/* (For use by those implementing new parser/parser-factories.) */
void dang_parser_factory_register (DangParserFactory *factory,
                                   const char        *name);
void dang_parser_base_init        (DangParser        *parser);
void dang_parser_push_expr        (DangParser        *parser,
                                   DangExpr          *expr);

/* like push_expr, but it does NOT incrment the ref-count on expr */
void dang_parser_take_expr        (DangParser        *parser,
                                   DangExpr          *expr);
void dang_parser_base_clear       (DangParser        *parser);


/* --- private --- */
typedef struct _DangParserDefault DangParserDefault;
struct _DangParserDefault
{
  DangParser base;
  void *lemon_parser;
  DangUtilArray errors;
  DangToken *last_token;

  /* the our-grammar-depends-on-knowing-the-type hack */
  DangUtilArray bareword_dot_arr;   /* for identifying types */

  /* the langle hack. TODO: documnet */
  dang_boolean look_for_langle;
  unsigned langle_count;
};
extern DangParserFactory dang_parser_factory_default;
