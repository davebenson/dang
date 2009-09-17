
struct _DangCodePosition
{
  unsigned line;
  DangString *filename;
};
#define DANG_CODE_POSITION_INIT { 0, NULL }

void dang_code_position_init (DangCodePosition *code_position);
void dang_code_position_copy (DangCodePosition *target,
                              DangCodePosition *source);
void dang_code_position_clear(DangCodePosition *code_position);


/* These can be used to print a code-position */
#define DANG_CP_FORMAT   "%s:%u"
#define DANG_CP_ARGS(cp)  ((cp).filename ? (cp).filename->str : "*no filename*"), (cp).line

/* For getting the code-position args from an expression */
#define DANG_CP_EXPR_ARGS(expr)  DANG_CP_ARGS(expr->any.code_position)
