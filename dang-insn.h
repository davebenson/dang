// NOTE: these are not handled b/c they never go through a function builder 
//dang-closure-factory.c:  dang_debug_register_run_func (step__closure_invoke,
//dang-closure-factory.c:  dang_debug_register_run_func (step__closure_finish,

typedef struct _DangInsn_Base DangInsn_Base;


/* Types of instructions. */
/* maintainer note: must match pack_funcs in dang_insn_pack.c */
typedef enum
{
  DANG_INSN_TYPE_INIT,
  DANG_INSN_TYPE_DESTRUCT,
  DANG_INSN_TYPE_ASSIGN,
  DANG_INSN_TYPE_JUMP,
  DANG_INSN_TYPE_JUMP_CONDITIONAL,
  DANG_INSN_TYPE_FUNCTION_CALL,
  DANG_INSN_TYPE_RUN_SIMPLE_C,
  DANG_INSN_TYPE_PUSH_CATCH_GUARD,
  DANG_INSN_TYPE_POP_CATCH_GUARD,
  DANG_INSN_TYPE_RETURN,
  DANG_INSN_TYPE_INDEX,

  DANG_INSN_TYPE_CREATE_CLOSURE,
  DANG_INSN_TYPE_NEW_TENSOR
} DangInsnType;


/* The location of a value */
typedef enum
{
  DANG_INSN_LOCATION_STACK,
  DANG_INSN_LOCATION_POINTER,
  DANG_INSN_LOCATION_GLOBAL,
  DANG_INSN_LOCATION_LITERAL
} DangInsnLocation;

typedef struct _DangInsnValue DangInsnValue;
struct _DangInsnValue
{
  DangInsnLocation location;    /* for all values */
  DangValueType *type;          /* for all values */
  DangVarId var;                /* for STACK and POINTER */
  unsigned offset;              /* for POINTER and GLOBAL */
  DangNamespace *ns;            /* for GLOBAL */
  void *value;                  /* for LITERAL */
};

void dang_insn_value_from_compile_result (DangInsnValue *out,
                                          DangCompileResult *in);
void dang_insn_value_copy  (DangInsnValue *out,
                            DangInsnValue *in);
void dang_insn_value_clear (DangInsnValue *out);

struct _DangInsn_Base
{
  DangInsnType insn_type;
  DangCodePosition cp;
};

typedef struct _DangInsn_Destruct DangInsn_Destruct;
struct _DangInsn_Destruct
{
  DangInsn_Base base;
  DangVarId var;
};

///typedef struct _DangInsn_VirtualGetFunction DangInsn_VirtualGetFunction;
///struct _DangInsn_VirtualGetFunction
///{
///  DangInsn_Base base;
///  DangVarId target;                     /* output variable */
///  DangInsnValue object;
///  DangValueType *object_type;
///  unsigned class_offset;                /* offset of function in class */
///};
///
typedef struct _DangInsn_Assign DangInsn_Assign;
struct _DangInsn_Assign           /* from dang_builder_add_assign() */
{
  DangInsn_Base base;
  DangInsnValue target, source;
  dang_boolean target_uninitialized;    /* only used if target.location==STACK */
};

/* note: this becomes two steps: a setup+invoke, and a copy-output-params and cleanup step */
typedef struct _DangInsn_FunctionCall DangInsn_FunctionCall;
struct _DangInsn_FunctionCall
{
  DangInsn_Base base;
  DangSignature *sig;
  DangVarId frame_var_id;
  DangInsnValue function;
  DangInsnValue *params;         /* return-value is params[0], if there is a retval */
};

typedef struct _DangInsn_Jump DangInsn_Jump;
struct _DangInsn_Jump           /* from dang_builder_add_jump */
{
  DangInsn_Base base;
  DangLabelId target;
};

typedef struct _DangInsn_JumpConditional DangInsn_JumpConditional;
struct _DangInsn_JumpConditional/* from dang_builder_add_jump */
{
  DangInsn_Base base;
  DangLabelId target;
  DangInsnValue test_value;
  dang_boolean jump_if_zero;
};

typedef struct _DangInsn_Init DangInsn_Init;
struct _DangInsn_Init           /* from dang_compile_obey_flags, mf-var_decl */
{
  DangInsn_Base base;
  DangVarId var;
};

typedef struct _DangInsn_RunSimpleC DangInsn_RunSimpleC;
struct _DangInsn_RunSimpleC     /* from dang_function_new_simple_c */
{
  DangInsn_Base base;
  DangFunction *func;
  DangInsnValue *args;       /* args[0] is the return-value, if non-void */
};

typedef struct _DangInsn_PushCatchGuard DangInsn_PushCatchGuard;
struct _DangInsn_PushCatchGuard /* from mf-catch */
{
  DangInsn_Base base;
  unsigned catch_block_index;
};

typedef struct _DangInsn_PopCatchGuard DangInsn_PopCatchGuard;
struct _DangInsn_PopCatchGuard  /* from mf-catch */
{
  DangInsn_Base base;
};

typedef struct _DangInsn_Return DangInsn_Return;
struct _DangInsn_Return         /* from dang_builder_add_return */
{
  DangInsn_Base base;
};

typedef struct _DangInsn_Index DangInsn_Index;
struct _DangInsn_Index         /* from mf-operator-index */
{
  DangInsn_Base base;
  DangInsnValue container;
  DangValueIndexInfo *index_info;
  DangInsnValue *indices;
  DangInsnValue element;
  dang_boolean is_set;
};

typedef struct _DangInsn_CreateClosure DangInsn_CreateClosure;
struct _DangInsn_CreateClosure    /* from dang_compile_create_closure() */
{
  DangInsn_Base base;
  DangVarId target;               /* where the new function goes */
  DangClosureFactory *factory;
  dang_boolean is_literal;
  union {
    DangFunction *literal;
    DangVarId function_var;
  } underlying;
  DangVarId *input_vars;
};

typedef struct _DangInsn_NewTensor DangInsn_NewTensor;
struct _DangInsn_NewTensor
{
  DangInsn_Base base;
  DangVarId target;
  DangValueType *elt_type;
  unsigned rank;
  unsigned *dims;
  unsigned total_size;
};

union _DangInsn
{
  DangInsnType type;
  DangInsn_Base base;

  DangInsn_Destruct destruct;
  //DangInsn_VirtualGetFunction virtual_get_function;
  DangInsn_Assign assign;
  DangInsn_CreateClosure create_closure;
  DangInsn_FunctionCall function_call;
  DangInsn_Jump jump;
  DangInsn_JumpConditional jump_conditional;
  DangInsn_Init init;
  DangInsn_RunSimpleC run_simple_c;
  DangInsn_PushCatchGuard push_catch_guard;
  DangInsn_PopCatchGuard pop_catch_guard;
  DangInsn_Return return_;
  DangInsn_Index index;
  DangInsn_NewTensor new_tensor;
};

void dang_insn_init (DangInsn *insn,
                     DangInsnType type);
void dang_insn_destruct (DangInsn *insn);



typedef struct _DangInsnLabelFixup DangInsnLabelFixup;
struct _DangInsnLabelFixup
{
  unsigned step_data_offset;
  DangLabelId label;
};
typedef struct _DangInsnDestroy DangInsnDestroy;
typedef void (*DangInsnDestroyNotify) (void *arg1, void *arg2);
struct _DangInsnDestroy
{
  dang_boolean is_step_data_destroy;
  DangInsnDestroyNotify func;
  unsigned offset;
  void *arg1;           /* if !is_step_data_destroy */
  void *arg2;
};

typedef struct
{
  DangArray step_data;
  DangArray label_fixups;
  DangArray destroys;

  /* reference to builder array, for convenience */
  unsigned n_vars;
  DangBuilderVariable *vars;
  DangBuilderLabel *labels;
} DangInsnPackContext;

void         dang_insn_pack (DangInsn             *insn,
                             DangInsnPackContext  *context);
void         dang_insn_dump (DangInsn *insn,
                             DangBuilderVariable *vars,
                             DangBuilderLabel *labels,
                             DangStringBuffer *out);
