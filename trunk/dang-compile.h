typedef unsigned DangStepNum;
#define DANG_STEP_NUM_INVALID   ((DangStepNum)(-1))


/* high-level api */
DangFunction *dang_compile_function (DangNamespace *ns,
                                     DangString    *filename,
                                     const char    *str,
                                     DangError    **error);

typedef enum
{
  DANG_COMPILE_RESULT_STACK,    /* result is on-stack */
  DANG_COMPILE_RESULT_ERROR,    /* error compiling */

  /* these are returned only if the user permits them
     with the permit_* members of DangCompileFlags.  */
  DANG_COMPILE_RESULT_POINTER,  /* stack has pointer to value */
  DANG_COMPILE_RESULT_GLOBAL,   /* global value (stored in ns?) */
  DANG_COMPILE_RESULT_LITERAL,  /* literal value */
  DANG_COMPILE_RESULT_VOID      /* no result, but successful compilation */

} DangCompileResultType;

struct _DangCompileFlags
{
  unsigned permit_global : 1,
           permit_pointer : 1,
           permit_literal : 1,
           prefer_void : 1,
           permit_void : 1,
           permit_untyped : 1,
           permit_uninitialized : 1,
           unused_return_value : 1,
           must_be_lvalue : 1,
           must_be_rvalue : 1;
};                                          /* g p l v v u u r l r */
#define DANG_COMPILE_FLAGS_LVALUE_PERMISSIVE  {1,1,0,0,0,1,1,0,1,0}
#define DANG_COMPILE_FLAGS_LVALUE_RESTRICTIVE {0,0,0,0,0,0,0,0,1,0}
#define DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE  {1,1,1,0,1,1,0,0,0,1}
#define DANG_COMPILE_FLAGS_LRVALUE_PERMISSIVE {1,1,0,0,1,1,0,0,1,1}
#define DANG_COMPILE_FLAGS_RVALUE_RESTRICTIVE {0,0,0,0,0,0,0,0,0,1}
#define DANG_COMPILE_FLAGS_VOID               {0,0,0,1,1,0,0,0,0,0}
extern DangCompileFlags dang_compile_flags_void;
extern DangCompileFlags dang_compile_flags_rvalue_restrictive;
extern DangCompileFlags dang_compile_flags_rvalue_permissive;
extern DangCompileFlags dang_compile_flags_lvalue_restrictive;
extern DangCompileFlags dang_compile_flags_lvalue_permissive;

typedef struct _DangCompileLock DangCompileLock;
struct _DangCompileLock
{
  DangVarId var;
  unsigned is_write_lock : 1;
  uint16_t n_member_accesses;
  DangCodePosition cp;
  DangCompileLock *next_in_result;
  DangCompileLock *next_in_builder;
  DangValueElement *member_accesses[1];  /* more may follow */
};

typedef struct
{
  DangCompileResultType type; /* must be first */
  DangValueType *return_type;
  dang_boolean is_lvalue;
  dang_boolean is_rvalue;
  DangCompileLock *lock_list;
} DangCompileResultAny;

typedef void         (*DangCompileLValueCallback)  (DangCompileResult   *result,
                                                    DangBuilder *builder);
typedef void         (*DangCompileLValueDestroy)   (void                *data,
                                                    DangBuilder *builder);

typedef struct 
{
  DangCompileResultAny base;
  DangVarId var_id;
  dang_boolean was_initialized;

  DangCompileLValueCallback lvalue_callback;
  void *callback_data;
  DangCompileLValueDestroy callback_data_destroy;
} DangCompileResultStack;

typedef struct
{
  DangCompileResultAny base;
  DangVarId var_id;
  unsigned offset;            /* offset in pointer of the value */
} DangCompileResultPointer;
typedef struct 
{
  DangCompileResultAny base;
  DangError *error;
} DangCompileResultError;

typedef struct 
{
  DangCompileResultAny base;
  DangNamespace *ns;
  unsigned ns_offset;
} DangCompileResultGlobal;

typedef struct
{
  DangCompileResultAny base;
  void *value;
} DangCompileResultLiteral;

union _DangCompileResult
{
  DangCompileResultType type;
  DangCompileResultAny any;
  DangCompileResultStack stack;
  DangCompileResultError error;
  DangCompileResultPointer pointer;
  DangCompileResultLiteral literal;
  DangCompileResultGlobal global;
};


void dang_compile_result_init_stack (DangCompileResult *to_init,
                                     DangValueType     *type,
                                     DangVarId          var_id,
                                     dang_boolean       was_initialized,
                                     dang_boolean       is_lvalue,
                                     dang_boolean       is_rvalue);
void dang_compile_result_init_pointer (DangCompileResult *to_init,
                                       DangValueType     *type,
                                       DangVarId          var_id,
                                       unsigned           offset,
                                       dang_boolean       is_lvalue,
                                       dang_boolean       is_rvalue);
void dang_compile_result_init_void    (DangCompileResult *to_init);

void dang_compile_result_init_global (DangCompileResult *result,
                                      DangValueType     *type,
                                      DangNamespace     *ns,
                                      unsigned           offset,
                                      dang_boolean       is_lvalue,
                                      dang_boolean       is_rvalue);
void dang_compile_result_init_literal (DangCompileResult *result,
                                       DangValueType  *type,
                                       const void     *value);
void dang_compile_result_init_literal_take (DangCompileResult *result,
                                       DangValueType  *type,
                                       void     *value);
void dang_compile_result_set_error (DangCompileResult *result,
                                    DangCodePosition  *pos,
                                    const char        *format,
                                    ...);
void dang_compile_result_set_error_builder
                                   (DangCompileResult *result,
                                    DangBuilder *builder,
                                    const char        *format,
                                    ...);
void dang_compile_result_steal_locks (DangCompileResult *dst,
                                      DangCompileResult *src);

/* if the lock fails, error is left in 'result' */
dang_boolean
     dang_compile_result_lock        (DangCompileResult *result,
                                      DangBuilder *builder,
                                      DangCodePosition    *cp,
                                      dang_boolean       is_write_lock,
                                      DangVarId          var_id,
                                      unsigned           n_member_accesses,
                                      DangValueElement  **member_accesses);

void dang_compile_result_clear       (DangCompileResult *result,
                                      DangBuilder *builder);



/* needed to write new control flow structures, etc. */
typedef void         (*DangCompileFunc)    (DangExpr              *expr,
                                            DangBuilder   *builder,
                                            void                  *func_data,
                                            DangCompileFlags      *flags,
                                            DangCompileResult     *result);

/* functions useful for implementing metafunctions */
void         dang_compile              (DangExpr                *expr,
                                        DangBuilder     *builder,
                                        DangCompileFlags        *flags,
                                        DangCompileResult       *result);
/* CAUTION: takes responsibility for cleanup cur_res */
void         dang_compile_member_access(DangBuilder     *builder, 
                                        DangCompileResult       *cur_res,
                                        DangValueType           *member_type,
                                        const char              *member_name,
                                        DangValueMember         *member,
                                        DangCompileFlags        *flags,
                                        DangCompileResult       *new_res);

#define dang_builder_add_jump_if_zero(builder, test_value, target) \
  dang_builder_add_conditional_jump(builder, test_value, TRUE, target)
#define dang_builder_add_jump_if_nonzero(builder, test_value, target) \
  dang_builder_add_conditional_jump(builder, test_value, FALSE, target)

/* compile a function in a way that can be used
   for SimpleC or Dang functions. */
void         dang_compile_function_invocation
                                       (DangCompileResult   *function,
                                        DangBuilder *builder,
                                        DangCompileResult   *return_value_info,
                                        unsigned             n_params,
                                        DangCompileResult   *params);
void dang_compile_literal_function_invocation
                                       (DangFunction        *function,
                                        DangBuilder *builder,
                                        DangCompileResult   *return_value_info,
                                        unsigned             n_params,
                                        DangCompileResult   *params);

/* Create a new function from an underlying function,
   by partial application at the end.
   In other words, given a function f: A,B,C,D -> E
   and, say, values c, d for C, D,
   return a function f': A,B -> E such that f(a,b,c,d) = f'(a,b). */
void        dang_compile_create_closure(DangBuilder     *builder,
                                        DangCodePosition        *cp,
                                        DangCompileResult       *func,
                                        unsigned                 n_vars,
                                        DangVarId               *vars,
                                        DangCompileResult       *result);

/* DEPRECATED: type_expr should be a value now: the type itself. */
DangValueType*
             dang_compile_type         (DangExpr                *type_expr,
                                        DangBuilder     *builder,
                                        DangError              **error);

void         dang_compile_obey_flags (DangBuilder *builder,
                              DangCompileFlags    *flags,
                              DangCompileResult   *result_inout);

void dang_compile_result_force_initialize (DangBuilder *builder,
                                           DangCompileResult   *result_inout);

/* to compile:
     int f(int a, int b)
     {
       int c = a * b;
       return c + square (a);
     }

  $function_define(int, f, $arguments($argument(int, a),
                                      $argument(int, b)),
                  $statement_list(
                    $assign($var_decl(int, c), multiply(a, b)),
                    $return(add(c, square(a)))
                  )
                  );
 */
/* $function_define(return_type, function_name, argument_set, expr)

      creates an anon function by setting up a lexical scope

   $arguments(), $argument(): no meaning outside function_define()

   $statement_list()

      setup a scope and compile each statement

   $var_decl()
      declare a variable; run code will zero the variable,

   $assign() -- treats its left argument as for an lvalue
      (compile_expr_lvalue() (ps this function should contain whether to
      zero the value)).
 */
