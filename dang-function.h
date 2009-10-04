
typedef void (*DangStepRun) (void                 *step_data,
                             DangThreadStackFrame *stack_frame,
                             DangThread           *thread);

#define DANG_STEP_RUN_DECLARE(func_name)             \
  void func_name (void                 *step_data,   \
                  DangThreadStackFrame *stack_frame, \
                  DangThread           *thread)

struct _DangStep
{
  DangStepRun func;
  unsigned _step_data_size;
  /* step data follows */
};

/* TODO: reimplement as inline function */
void dang_thread_stack_frame_advance_ip(DangThreadStackFrame *frame,
                            size_t      sizeof_step_data);

typedef void         (*DangFunctionCompileFunc)(DangFunction        *function,
                                                DangBuilder *builder,
                                                DangCompileResult   *return_value_info,
                                                unsigned             n_params,
                                                DangCompileResult   *params);

typedef struct _DangFunctionBase DangFunctionBase;
typedef struct _DangFunctionSimpleC DangFunctionSimpleC;
typedef struct _DangFunctionC DangFunctionC;
typedef struct _DangFunctionDang DangFunctionDang;


typedef enum
{
  DANG_FUNCTION_TYPE_DANG,
  DANG_FUNCTION_TYPE_SIMPLE_C,
  DANG_FUNCTION_TYPE_STUB,
  DANG_FUNCTION_TYPE_CLOSURE,
  DANG_FUNCTION_TYPE_NEW_OBJECT
} DangFunctionType;
const char *dang_function_type_name (DangFunctionType type);

typedef struct _DangCatchBlockClause DangCatchBlockClause;
typedef struct _DangCatchBlock DangCatchBlock;
typedef struct _DangFunctionStackVarInfo DangFunctionStackVarInfo;
typedef struct _DangFunctionStackParamInfo DangFunctionStackParamInfo;
typedef struct _DangFunctionStackInfo DangFunctionStackInfo;
struct _DangFunctionStackVarInfo
{
  DangStep *start, *end;                /* NOT LIVE when going to 'start';
                                           LIVE when going to end */
  unsigned offset;
  DangValueType *type;
};
struct _DangFunctionStackParamInfo
{
  unsigned offset;
  DangValueType *type;
};

struct _DangCatchBlockClause
{
  DangValueType *type;
  DangStep *catch_target;
  unsigned catch_var_offset;
};
struct _DangCatchBlock
{
  DangStep *start, *end;                 /* NOT ACTIVE when going to 'start';
                                            ACTIVE when going to 'end' */
  unsigned n_clauses;
  DangCatchBlockClause *clauses;
};
dang_boolean dang_catch_block_is_applicable (DangCatchBlock *catch_block,
                                             DangValueType  *thrown_type,
                                             DangCatchBlockClause **which_clause_out);

typedef struct _DangFunctionLineInfo DangFunctionLineInfo;
struct _DangFunctionLineInfo
{
  unsigned line;
  DangStep *step;
};
typedef struct _DangFunctionFileInfo DangFunctionFileInfo;
struct _DangFunctionFileInfo
{
  DangString *filename;
  unsigned n_line_infos;
  DangFunctionLineInfo line_infos[1];   /* more follow */
};

struct _DangFunctionStackInfo
{
  unsigned n_vars;
  DangFunctionStackVarInfo *vars;
  unsigned n_params;
  DangFunctionStackParamInfo *params;
  unsigned n_catch_blocks;
  DangCatchBlock *catch_blocks;
  DangStep *first_step, *last_step;
  unsigned n_file_info;
  DangFunctionFileInfo **file_info;
};

struct _DangFunctionBase
{
  DangFunctionType type;
  unsigned ref_count;
  DangFunctionCompileFunc compile;
  DangFunctionStackInfo *stack_info;
  DangSignature *sig;
  unsigned frame_size;
  DangStep *steps;              /* not a normal array- mixed with data */
  dang_boolean is_owned;
};

typedef struct {
  void (*func)(void *arg1, void *arg2);
  void *arg1, *arg2;
} DangFunctionDangDestruct;

struct _DangFunctionDang
{
  DangFunctionBase base;

  /* Step information that must be destroyed
     when this function is destroyed. */
  unsigned n_destroy;
  DangFunctionDangDestruct *destroy;
};

/* This is only used for native dang objects. */
typedef struct _DangFunctionNewObject DangFunctionNewObject;
struct _DangFunctionNewObject
{
  DangFunctionBase base;

  DangStep step;
  DangValueType *object_type;
  DangFunction *constructor;
  DangCompileContext *cc;
  dang_boolean must_unref_constructor;
};

typedef dang_boolean (*DangSimpleCFunc) (void      **args,
                                         void       *rv_out,
                                         void       *func_data,
                                         DangError **error);
#define DANG_SIMPLE_C_FUNC_DECLARE(func_name) \
    dang_boolean func_name (void      **args, \
                            void       *rv_out, \
                            void       *func_data, \
                            DangError **error)


struct _DangFunctionSimpleC
{
  DangFunctionBase base;
  DangSimpleCFunc func;
  void *func_data;
  DangDestroyNotify func_data_destroy;
};

typedef enum
{
  DANG_C_FUNCTION_SUCCESS,
  DANG_C_FUNCTION_YIELDED,
  DANG_C_FUNCTION_ERROR
} DangCFunctionResult;

typedef DangCFunctionResult (*DangCFunc)     (void      **args,
                                              void       *rv_out,
                                              void       *state_data,
                                              void       *func_data,
                                              DangError **error);
#define DANG_C_FUNC_DECLARE(func_name)                                \
        DangCFunctionResult       func_name  (void      **args,       \
                                              void       *rv_out,     \
                                              void       *state_data, \
                                              void       *func_data,  \
                                              DangError **error)

struct _DangFunctionC
{
  DangFunctionBase base;
  DangValueType *state_type;
  DangCFunc func;
  void *func_data;
  DangDestroyNotify func_data_destroy;
};

typedef struct _DangFunctionStub DangFunctionStub;
struct _DangFunctionStub
{
  DangFunctionBase base;
  DangImports *imports;
  DangExpr *body;
  DangCompileContext *cc;               /* if being compiled */
  DangValueType *method_type;           /* if a method */
  unsigned n_friends;
  DangValueType **friends;
  DangAnnotations *annotations;
  DangVarTable *var_table;
};

typedef struct _DangFunctionClosure DangFunctionClosure;
struct _DangFunctionClosure
{
  DangFunctionBase base;
  DangFunction *underlying;
  DangClosureFactory *factory;
  DangStep steps[2];
};

union _DangFunction
{
  DangFunctionType type;
  DangFunctionBase base;
  DangFunctionDang dang;
  DangFunctionSimpleC simple_c;
  DangFunctionC c;
  DangFunctionClosure closure;
  DangFunctionNewObject new_object;
  DangFunctionStub stub;
};

/* Adds this function to the compile context. (!) */
DangFunction *dang_function_new_stub     (DangImports     *imports,
                                          DangSignature   *sig,
                                          DangExpr        *body,
                                          DangValueType   *method_type,
                                          unsigned         n_friends,
                                          DangValueType  **friends);
/* takes ownership of annotations and var_table */
void dang_function_stub_set_annotations (DangFunction *function,
                                         DangAnnotations *annotations,
                                         DangVarTable *var_table);
DangFunction *dang_function_new_simple_c (DangSignature   *sig,
                                          DangSimpleCFunc  func,
                                          void            *func_data,
                                          DangDestroyNotify func_data_destroy);
DangFunction *dang_function_new_c        (DangSignature   *sig,
                                          DangValueType   *state_type,
                                          DangCFunc        func,
                                          void            *func_data,
                                          DangDestroyNotify func_data_destroy);
void          dang_function_unref        (DangFunction    *function);
DangFunction *dang_function_ref          (DangFunction    *function);

/* one-line typed-function name */
char *dang_function_to_string (DangFunction *);

/* increase the ref-count by 1 and mark it attached */
DangFunction *dang_function_attach_ref   (DangFunction    *function);

DangFunction *dang_function_concat_get_global (void);

dang_boolean dang_function_needs_registration (DangFunction *function);

dang_boolean dang_function_get_code_position (DangFunction *function,
                                              DangStep     *step,
                                              DangCodePosition *pos_out);
dang_boolean dang_function_call_nonyielding_v (DangFunction *function,
                                               void         *rv_value,
                                               void        **arg_values,
                                               DangError   **error);
