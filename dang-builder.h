typedef enum
{
  DANG_FUNCTION_BUILDER_LABEL_TYPE_TMP,
  DANG_FUNCTION_BUILDER_LABEL_TYPE_NAMED,
  DANG_FUNCTION_BUILDER_LABEL_TYPE_SCOPED
} DangBuilderLabelType;

struct _DangBuilderLabel
{
  DangBuilderLabelType type;
  char *name;           /* for named + scoped */
  DangStepNum target;                /* or DANG_CODE_OFFSET_INVALID */
  DangStepNum first_active, last_active;
  unsigned scope_index;

  DangCodePosition first_goto_position;
  DangCodePosition definition_position;
};

struct _DangBuilderVariable
{
  DangValueType *type;
  char *name;           /* if applicable */
  unsigned offset;      /* once variables are allocated */

  /* Is this variable a parameter or return-value? */
  dang_boolean is_param;
  DangFunctionParamDir param_dir;

  /* A variable must be initialized when jumping to step S
   * if start < S <= end.  When jumping from S,
   * you must cleanup variables if start <= S < end.
   * In other words, variables must be initialized / destroyed BEFORE
   * any jumps occur. */
  DangStepNum start;
  DangStepNum end;           /* or DANG_STEP_NUM_INVALID */

  /* For locals that are just aliases to another variable
     (e.g. aliases inside a structure) */
  DangVarId container;       /* or DANG_VAR_ID_INVALID */
  unsigned container_offset;

  dang_boolean bound;
};

/* a scope is a container of local or tmp variables;
   local and tmp vars have somewhat different destruction policies,
   so they have different scopes which behave independently. */
typedef struct _DangBuilderScope DangBuilderScope;
struct _DangBuilderScope
{
  DangArray var_ids;
  DangBuilderScope *up;
};

typedef struct _DangBuilderFriend DangBuilderFriend;
struct _DangBuilderFriend
{
  DangValueType *type;
  dang_boolean private_access;          /* versus just protected access */
  DangBuilderFriend *next;
};

typedef struct _DangBuilder DangBuilderLabelScope;
typedef struct _DangBuilderScopedLabel DangBuilderScopedLabel;
struct _DangBuilderScopedLabel
{
  char *name;
  DangArray labels;             /* of DangLabelId, used like a stack */
};

typedef struct _DangBuilderCatchClause DangBuilderCatchClause;
typedef struct _DangBuilderCatchBlock DangBuilderCatchBlock;
struct _DangBuilderCatchClause
{
  DangValueType *type;
  DangLabelId target;

  /* the location to put the throw entity. */
  DangVarId var_id;
};
struct _DangBuilderCatchBlock
{
  /* You may assume that 'start' adds the catch block
     and 'end' removes the catch block, once
     the end has been set (it's DANG_CODE_OFFSET_INVALID until then).
     Therefore, there should never be throw stmt that jumps
     FROM one of these.  If jumping INTO a catch block
     and DangStepNum IP, then the block must be added
     if start < IP <= end. */
  DangStepNum start, end;

  size_t n_clauses;
  DangBuilderCatchClause *clauses;
};

struct _DangBuilder
{
  DangAnnotations *annotations;
  DangFunction *function;       /* initially a FunctionStub, converted to a FunctionDang by
                                   dang_builder_compile() */
  DangArray insns;              /* of DangInsn */
  DangArray labels;             /* of DangBuilderLabel */
  DangArray scoped_labels;      /* of DangBuilderScopedLabel */

  /* All variables: named, temporary or aliased:
       named:       user-named variable
       temporary:   unnamed variable allocated to implement something
       aliased:     pointer into a named or temporary variable
   */
  DangArray vars;               /* of DangBuilderVariable */

  /* the return value is always variable (DangVarId)0.  */
  dang_boolean has_return_value;
  DangValueType *return_type;

  /* stack of scopes for named variables */
  DangBuilderScope *local_scope;

  /* stack of scopes for unnamed variables */
  DangBuilderScope *tmp_scope;

  /* array of all catch blocks for this function */
  DangArray catch_blocks;

  DangSignature *sig;
  DangImports *imports;

  /* used for checking privacy constraints */
  DangBuilderFriend *friend_stack;

  /* need a return statement to follow the last step */
  dang_boolean needs_return;

  /* used for providing an approximate position if we have
     no idea where to attribute the problem. */
  DangCodePosition pos;

  DangCompileLock *locks;
};

DangBuilder* dang_builder_new           (DangFunction        *stub,
                                         DangVarTable        *var_table,
                                         DangAnnotations     *annotations);

void         dang_builder_set_pos       (DangBuilder         *builder,
                                         DangCodePosition    *code_position);

/* Add an instruction to the list */
void         dang_builder_add_insn      (DangBuilder         *builder,
                                         DangInsn            *insn);


/* add certain types of INSN */
void         dang_builder_add_assign    (DangBuilder         *builder,
                                         DangCompileResult   *lvalue,
                                         DangCompileResult   *rvalue);
void         dang_builder_add_return    (DangBuilder         *builder);
void         dang_builder_add_jump      (DangBuilder         *builder,
                                         DangLabelId          target);
void         dang_builder_add_conditional_jump
                                        (DangBuilder         *builder,
                                         DangCompileResult   *test_value,
                                         dang_boolean         jump_if_zero,
                                         DangLabelId          target);

typedef unsigned DangCatchBlockId;
/* takes ownership of clauses. */
DangCatchBlockId dang_builder_start_catch_block (DangBuilder *builder,
                                         unsigned n_clauses,
                                         DangBuilderCatchClause *clauses);
void             dang_builder_end_catch_block (DangBuilder *builder,
                                         DangCatchBlockId id);
 

/* There are three types of labels:
 * - named labels -- only one per name per function
 * - anonymous labels -- used by the code generator
 * - scoped labels -- a stack of labels for implementing control flow
 */

DangLabelId  dang_builder_find_named_label(DangBuilder       *builder,
                                         const char          *name,
                                         DangCodePosition    *goto_position);
DangLabelId  dang_builder_start_scoped_label
                                        (DangBuilder         *builder,
                                         const char          *name);
void         dang_builder_end_scoped_label
                                        (DangBuilder         *builder,
                                         DangLabelId          label);

/* Default level is 0, meaning the current active scoped level.
   1 means the next level, etc.
   We use "1"-based level indications for the user, a bit of a hack
   in the parser... b/c i want it to be "the first level" or
   how many levels to jump out, in the case of a break statement. */
DangLabelId  dang_builder_find_scoped_label
                                               (DangBuilder *builder,
                                                const char          *name,
                                                unsigned             level);

DangLabelId  dang_builder_create_label(DangBuilder *builder);
DangLabelId  dang_builder_create_label_at(DangBuilder *builder,
                                                   DangStepNum target);

/* defines it at the next instruction we lay down. */
void         dang_builder_define_label  (DangBuilder *builder,
                                         DangLabelId          label);
dang_boolean dang_builder_is_label_defined (DangBuilder *bu,
                                         DangLabelId          label);


/* local variables */
dang_boolean  dang_builder_lookup_local (DangBuilder *builder,
                                         const char *name,
                                         dang_boolean last_scope_only,
                                         DangVarId   *var_id_out);
void          dang_builder_add_local    (DangBuilder *builder,
                                         const char *name,
                                         DangVarId var_id,
                                         DangValueType *type);
DangVarId     dang_builder_add_tmp      (DangBuilder *builder,
                                         DangValueType *type);
void          dang_builder_bind_local_var(DangBuilder *builder,
                                         const char *opt_name,
                                         DangVarId var_id);
void          dang_builder_bind_local_type(DangBuilder *builder,
                                         DangVarId var_id,
                                         DangValueType *type);

DangVarId     dang_builder_add_local_alias(DangBuilder *builder,
                                         DangVarId container,
                                         unsigned  offset,
                                         DangValueType *member_type);

DangValueType*dang_builder_get_var_type (DangBuilder         *builder,
                                         DangVarId            var_id);
const char   *dang_builder_get_var_name (DangBuilder         *builder,
                                         DangVarId            var_id);
DangValueType*dang_builder_is_param     (DangBuilder         *builder,
                                         DangVarId            var_id,
                                         DangFunctionParamDir*dir_out);
void          dang_builder_push_local_scope(DangBuilder *);
void          dang_builder_push_tmp_scope  (DangBuilder *);
void          dang_builder_pop_local_scope (DangBuilder *);
void          dang_builder_pop_tmp_scope   (DangBuilder *);

void       dang_builder_note_var_create (DangBuilder         *builder,
                                         DangVarId            id);
void      dang_builder_note_var_destruct(DangBuilder         *builder,
                                         DangVarId            id);

void          dang_builder_push_friend  (DangBuilder         *builder,
                                         DangValueType       *type,
                                         dang_boolean         access_private);
void          dang_builder_pop_friend   (DangBuilder         *builder);
dang_boolean  dang_builder_check_member_access(DangBuilder   *builder,
                                         DangValueType       *type,
                                         DangMemberFlags      flags,
                                         dang_boolean         read_access,
                                         dang_boolean         write_access,
                                         DangError          **error);
dang_boolean  dang_builder_check_method_access
                                        (DangBuilder         *builder,
                                         DangValueType       *type,
                                         DangMethodFlags      flags,
                                         DangError          **error);

/* If 'source' is DANG_STEP_NUM_INVALID, we report
   all variables that need initialization at target.
 * If 'target' is DANG_STEP_NUM_INVALID, we report
   all variables that need clean when leaving at 'source'.
 * It is not allowed for both source + target to be DANG_STEP_NUM_INVALID.
 * Otherwise (if source + target are valid)
   report variables that need cleanup or initialization,
   without the intersection.
 */
void          dang_builder_query_vars   (DangBuilder *,
                                         DangStepNum      source,
                                         DangStepNum      target,
                                         unsigned *n_destruct_out,
                                         DangVarId **destruct_out,
                                         unsigned *n_init_out,
                                         DangVarId **init_out);



/* Convert the builder into a function */
dang_boolean  dang_builder_compile      (DangBuilder      *builder,
                                         DangError          **error);

void          dang_builder_destroy    (DangBuilder *builder);

