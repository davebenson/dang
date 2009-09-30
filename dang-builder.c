#include <string.h>
#include "dang.h"

/* Shortcut to make the code more legible */
typedef DangBuilder            Builder;
typedef DangBuilderVariable    Variable;
typedef DangBuilderLabel       Label;
typedef DangBuilderScopedLabel ScopedLabel;
typedef DangBuilderCatchBlock  CatchBlock;
typedef DangBuilderScope       Scope;
#define GET_VAR(builder, var_id)  \
  DANG_UTIL_ARRAY_INDEX_PTR (&(builder)->vars, Variable, (var_id))

#if 0
static void
add_param (Builder *builder,
           const char *name,
           DangValueType *type)
{
  DangVarId id = dang_builder_add_local (builder, name, type);
  GET_VAR (builder, id)->is_param = TRUE;
}
#endif

/**
 * dang_builder_new:
 * @stub: the function to compile
 * @var_table: the known named variables and parameters.
 * @annotations: table of information about each subexpression.
 *
 * Create a new function "builder".  The builder consists
 * of an array of instructions (DangInsn), a table of variables (named and unnamed),
 * a set of labels (possibly untargetted jump destinations).
 *
 * returns: a new function builder.
 */
DangBuilder*
dang_builder_new         (DangFunction        *stub,
                          DangVarTable        *var_table,
                          DangAnnotations     *annotations)
{
  Builder *builder = dang_new (Builder, 1);
  DangSignature *sig = stub->base.sig;
  DangImports *imports = stub->stub.imports;
  dang_assert (stub->type == DANG_FUNCTION_TYPE_STUB);
  dang_assert (sig != NULL);
  unsigned i;

  DANG_UTIL_ARRAY_INIT (&builder->insns, DangInsn);
  DANG_UTIL_ARRAY_INIT (&builder->labels, Label);
  DANG_UTIL_ARRAY_INIT (&builder->scoped_labels, ScopedLabel);
  DANG_UTIL_ARRAY_INIT (&builder->vars, Variable);
  DANG_UTIL_ARRAY_INIT (&builder->catch_blocks, CatchBlock);
  builder->function = dang_function_ref (stub);
  builder->annotations = annotations;

  builder->return_type = sig->return_type;
  builder->has_return_value = (sig->return_type != NULL);
  builder->sig = dang_signature_ref (sig);

  builder->local_scope = NULL;
  builder->tmp_scope = NULL;
  builder->friend_stack = NULL;

  if (imports == NULL)
    builder->imports = NULL;
  else
    builder->imports = dang_imports_ref (imports);

  dang_code_position_init (&builder->pos);
  builder->needs_return = TRUE;

  dang_builder_push_local_scope (builder);
  dang_util_array_set_size (&builder->vars, var_table->variables.len);
  for (i = 0; i < var_table->variables.len; i++)
    {
      DangVarTableVariable *in = ((DangVarTableVariable*) var_table->variables.data) + i;
      Variable *var = GET_VAR (builder, i);
      var->type = in->type;
      var->name = dang_strdup (in->name);
      var->offset = 0;
      var->is_param = in->is_param;
      var->param_dir = in->param_dir;
      if (var->is_param)
        {
          DangVarId var_id = i;
          dang_util_array_append (&builder->local_scope->var_ids, 1, &var_id);
        }
      var->start = DANG_STEP_NUM_INVALID;
      var->end = DANG_STEP_NUM_INVALID;
      var->container = DANG_VAR_ID_INVALID;
      var->container_offset = 0;
      var->bound = var->is_param;
    }
  dang_builder_push_tmp_scope (builder);
  builder->locks = NULL;

  return builder;
}

/**
 * dang_builder_is_param:
 * @builder: the builder to query.
 * @id: the variable id to inquire about.
 * @dir_out: optional place to store the parameters direction (in, out or inout).
 *
 * Detect if the variable is a function parameter,
 * including the return_value (which is always var-id 0 with direction 'out').
 *
 * returns: the type of the parameter, or NULL if the variable is not a parameter.
 */
DangValueType *
dang_builder_is_param (DangBuilder *builder,
                                DangVarId            id,
                                DangFunctionParamDir *dir_out)
{
  Variable *var = GET_VAR (builder, id);
  if (!var->is_param)
    return NULL;
  if (dir_out)
    *dir_out = var->param_dir;
  return var->type;
}

void
dang_builder_set_pos     (DangBuilder *builder,
                                   DangCodePosition    *position)
{
  dang_code_position_clear (&builder->pos);
  dang_code_position_copy (&builder->pos, position);
}

void
dang_builder_add_insn    (DangBuilder *builder,
                          DangInsn    *insn)
{
  DangInsn insn_copy = *insn;
  if (insn_copy.base.cp.filename == NULL)
    dang_code_position_copy (&insn_copy.base.cp, &builder->pos);
  dang_util_array_append (&builder->insns, 1, &insn_copy);
  builder->needs_return = TRUE;
}


void         dang_builder_add_assign   (DangBuilder     *builder,
                                        DangCompileResult       *lvalue,
                                        DangCompileResult       *rvalue)
{
  DangInsn insn;
  dang_assert (dang_value_type_is_autocast (lvalue->any.return_type, rvalue->any.return_type));
  dang_insn_init (&insn, DANG_INSN_TYPE_ASSIGN);
  dang_insn_value_from_compile_result (&insn.assign.target, lvalue);
  dang_insn_value_from_compile_result (&insn.assign.source, rvalue);
  insn.assign.target_uninitialized = lvalue->type == DANG_COMPILE_RESULT_STACK
                                  && !lvalue->stack.was_initialized;
  if (insn.assign.target_uninitialized)
    {
      dang_builder_note_var_create (builder, lvalue->stack.var_id);
    }
  dang_builder_add_insn (builder, &insn);

  if (lvalue->type == DANG_COMPILE_RESULT_STACK
   && lvalue->stack.lvalue_callback != NULL)
    {
      lvalue->stack.lvalue_callback (lvalue, builder);
    }
}

void
dang_builder_add_return (DangBuilder *builder)
{
  DangInsn insn;
  dang_insn_init (&insn, DANG_INSN_TYPE_RETURN);
  dang_builder_add_insn (builder, &insn);
  builder->needs_return = FALSE;
}


void
dang_builder_add_jump     (DangBuilder             *builder,
                           DangLabelId              target)
{
  DangInsn insn;
  dang_assert (target < builder->labels.len);
  dang_insn_init (&insn, DANG_INSN_TYPE_JUMP);
  insn.jump.target = target;
  dang_builder_add_insn (builder, &insn);
}

void
dang_builder_add_conditional_jump (DangBuilder             *builder,
                                   DangCompileResult       *test_value,
                                   dang_boolean             jump_if_zero,
                                   DangLabelId              target)
{
  DangInsn insn;
  dang_assert (target < builder->labels.len);

  /* Dispense with the "switch on literal" case */
  if (test_value->type == DANG_COMPILE_RESULT_LITERAL)
    {
      dang_boolean do_jump;
      if (dang_util_is_zero (test_value->literal.value,
                             test_value->any.return_type->sizeof_instance))
        do_jump = jump_if_zero;
      else
        do_jump = !jump_if_zero;
      if (do_jump)
        dang_builder_add_jump (builder, target);
      return;
    }

  dang_insn_init (&insn, DANG_INSN_TYPE_JUMP_CONDITIONAL);
  insn.jump_conditional.target = target;
  dang_insn_value_from_compile_result (&insn.jump_conditional.test_value, test_value);
  insn.jump_conditional.jump_if_zero = jump_if_zero;
  dang_builder_add_insn (builder, &insn);
}



static void
label_init (Label *label,
            DangBuilderLabelType type)
{
  label->name = NULL;
  label->type = type;
  label->target = DANG_STEP_NUM_INVALID;
  label->first_active = DANG_STEP_NUM_INVALID;
  label->last_active = DANG_STEP_NUM_INVALID;
  dang_code_position_init (&label->first_goto_position);
  dang_code_position_init (&label->definition_position);
}

DangLabelId
dang_builder_find_named_label            (DangBuilder *builder,
                                                   const char          *name,
                                                   DangCodePosition    *goto_position)
{
  unsigned i;
  unsigned n_labels = builder->labels.len;
  Label *labels = builder->labels.data;
  Label new_label;

  /* TODO: some kind of data structures :) */
  for (i = 0; i < n_labels; i++)
    if (labels[i].type == DANG_FUNCTION_BUILDER_LABEL_TYPE_NAMED
     && strcmp (labels[i].name, name) == 0)
      goto handle_goto_position;

  label_init (&new_label, DANG_FUNCTION_BUILDER_LABEL_TYPE_NAMED);
  new_label.name = dang_strdup (name);
  dang_util_array_append (&builder->labels, 1, &new_label);
  dang_code_position_init (&new_label.first_goto_position);
  dang_code_position_init (&new_label.definition_position);
  labels = builder->labels.data;                /* re-initialize after append */

handle_goto_position:

  /* If we are calling this due to a goto, and
     this is the first time that has happened,
     set the 'first_goto' information.  */
  if (goto_position != NULL
   && labels[i].first_goto_position.filename == NULL)
    dang_code_position_copy (&labels[i].first_goto_position, goto_position);

  return i;
}

DangLabelId
dang_builder_find_scoped_label (DangBuilder *builder,
                                         const char          *name,
                                         unsigned             level)
{
  unsigned n_scoped_labels = builder->scoped_labels.len;
  unsigned i;
  ScopedLabel *scoped_labels = builder->scoped_labels.data;
  unsigned index;
  for (i = 0; i < n_scoped_labels; i++)
    if (strcmp (scoped_labels[i].name, name) == 0)
      break;
  if (i == n_scoped_labels)
    return DANG_LABEL_ID_INVALID;
  if (level >= scoped_labels[i].labels.len)
    return DANG_LABEL_ID_INVALID;
  index = scoped_labels[i].labels.len - 1 - level;
  return ((DangLabelId*)scoped_labels[i].labels.data)[index];
}

DangLabelId
dang_builder_start_scoped_label (DangBuilder *builder,
                                          const char          *name)
{
  unsigned i;
  unsigned n_scoped_labels = builder->scoped_labels.len;
  ScopedLabel *scoped_labels = builder->scoped_labels.data;
  Label new_label;
  DangLabelId rv;
  for (i = 0; i < n_scoped_labels; i++)
    if (strcmp (scoped_labels[i].name, name) == 0)
      break;
  if (i == n_scoped_labels)
    {
      ScopedLabel scoped_label;
      scoped_label.name = dang_strdup (name);
      DANG_UTIL_ARRAY_INIT (&scoped_label.labels, DangLabelId);
      dang_util_array_append (&builder->scoped_labels, 1, &scoped_label);
      scoped_labels = builder->scoped_labels.data;  /* re-initialize after append */
      n_scoped_labels = builder->scoped_labels.len;  /* re-initialize after append */
    }

  /* allocate a label-id and assign it to the current ip */
  rv = builder->labels.len;
  label_init (&new_label, DANG_FUNCTION_BUILDER_LABEL_TYPE_SCOPED);
  new_label.name = dang_strdup (name);
  new_label.first_active = builder->insns.len;
  new_label.scope_index = i;
  dang_util_array_append (&builder->labels, 1, &new_label);

  /* push label_id */
  dang_util_array_append (&scoped_labels[i].labels, 1, &rv);

  /* return the label: it has no associated step-num */
  return rv;
}


DangLabelId
dang_builder_create_label (DangBuilder *builder)
{
  DangBuilderLabel label;
  DangLabelId id = builder->labels.len;
  label_init (&label, DANG_FUNCTION_BUILDER_LABEL_TYPE_TMP);
  dang_util_array_append (&builder->labels, 1, &label);
  return id;
}

DangLabelId
dang_builder_create_label_at (DangBuilder *builder,
                                       DangStepNum          step)
{
  DangBuilderLabel label;
  DangLabelId id = builder->labels.len;
  label_init (&label, DANG_FUNCTION_BUILDER_LABEL_TYPE_TMP);
  label.target = step;
  dang_util_array_append (&builder->labels, 1, &label);
  return id;
}


void
dang_builder_end_scoped_label (DangBuilder *builder,
                                        DangLabelId          label)
{
  DangLabelId last;
  DangArray *sl_arr = &builder->scoped_labels;
  ScopedLabel *sl;
  Label *lab = ((Label*)builder->labels.data) + label;
  dang_assert (lab->type == DANG_FUNCTION_BUILDER_LABEL_TYPE_SCOPED);
  sl = (ScopedLabel*)(sl_arr->data) + lab->scope_index;
  last = ((DangLabelId*)(sl->labels.data))[sl->labels.len - 1];
  dang_assert (last == label);
  dang_util_array_set_size (&sl->labels, sl->labels.len - 1);
  dang_assert (lab->last_active == DANG_STEP_NUM_INVALID);
  lab->last_active = builder->insns.len;
  if (lab->target == DANG_STEP_NUM_INVALID)
    dang_warning ("scoped label '%s' without target after being closed",
                  lab->name);
}

void
dang_builder_define_label(DangBuilder *builder,
                                   DangLabelId          label)
{
  Label *lab;
  dang_assert (label < builder->labels.len);
  lab = ((Label*)builder->labels.data) + label;
  dang_assert (lab->target == DANG_STEP_NUM_INVALID);
  lab->target = builder->insns.len;
  builder->needs_return = TRUE;
}

dang_boolean
dang_builder_is_label_defined (DangBuilder *builder,
                               DangLabelId          label)
{
  Label *lab;
  dang_assert (label < builder->labels.len);
  lab = ((Label*)builder->labels.data) + label;
  return (lab->target != DANG_STEP_NUM_INVALID);
}


dang_boolean
dang_builder_lookup_local  (DangBuilder *builder,
                                     const char *name,
                                     dang_boolean last_scope_only,
                                     DangVarId   *var_id_out)
{
  Scope *scope = builder->local_scope;
  while (scope != NULL)
    {
      DangVarId *ids = scope->var_ids.data;
      unsigned i;
      for (i = 0; i < scope->var_ids.len; i++)
        if (strcmp (GET_VAR (builder, ids[i])->name, name) == 0)
          {
            *var_id_out = ids[i];
            return TRUE;
          }
      if (last_scope_only)
        return FALSE;
      scope = scope->up;
    }
  return FALSE;
}

void
dang_builder_add_local     (DangBuilder *builder,
                                     const char *name,
                                     DangVarId   var_id,
                                     DangValueType *type)
{
  Variable *var = GET_VAR (builder, var_id);
  Scope *scope = builder->local_scope;
  dang_assert (strcmp (var->name, name) == 0);
  dang_assert (type == NULL || var->type == type);
  dang_assert (!var->bound);
  var->bound = TRUE;
  dang_util_array_append (&scope->var_ids, 1, &var_id);
}

DangVarId
dang_builder_add_tmp       (DangBuilder *builder,
                                     DangValueType *type)
{
  Variable var;
  Scope *scope = builder->tmp_scope;
  DangVarId var_id;
  dang_assert (scope != NULL);

  /* Allocate variable */
  var.type = type;
  var.name = NULL;
  var.offset = 0;               /* unused at this phase */
  var.is_param = FALSE;
  var.start = DANG_STEP_NUM_INVALID;
  var.end = DANG_STEP_NUM_INVALID;
  var.container = DANG_VAR_ID_INVALID;
  var.bound = TRUE;
  var.container_offset = 0;
  var_id = builder->vars.len;
  dang_util_array_append (&builder->vars, 1, &var);
  dang_util_array_append (&scope->var_ids, 1, &var_id);
  return var_id;
}

void
dang_builder_bind_local_type (DangBuilder *builder,
                                       DangVarId var_id,
                                       DangValueType *type)
{
  Variable *var;
  dang_assert (var_id < builder->vars.len);
  var = GET_VAR (builder, var_id);
  dang_assert (var->type == NULL || var->type == type);
  var->type = type;
}
void
dang_builder_bind_local_var (DangBuilder *builder,
                                      const char *opt_name,
                                      DangVarId var_id)
{
  Variable *var;
  Scope *scope = builder->local_scope;
  dang_assert (var_id < builder->vars.len);
  var = GET_VAR (builder, var_id);
  dang_assert (!var->bound);
  if (opt_name)
    scope = builder->local_scope;
  else
    scope = builder->tmp_scope;
  dang_assert (scope != NULL);
  if (var->name && opt_name)
    dang_assert (strcmp (var->name, opt_name) == 0);
  else if (var->name == NULL && opt_name != NULL)
    var->name = dang_strdup (opt_name);
  var->bound = TRUE;
  dang_util_array_append (&scope->var_ids, 1, &var_id);
}

DangVarId
dang_builder_add_local_alias (DangBuilder *builder,
                                       DangVarId container,
                                       unsigned  offset,
                                       DangValueType *member_type)
{
  DangVarId rv;
  Variable *var;
  dang_assert (member_type != NULL);
  rv = dang_builder_add_tmp (builder, member_type);
  var = GET_VAR (builder, rv);
  var->container = container;
  var->container_offset = offset;
  return rv;
}

DangValueType*
dang_builder_get_var_type    (DangBuilder *builder,
                                       DangVarId            var_id)
{
  dang_assert (var_id < builder->vars.len);
  return GET_VAR (builder, var_id)->type;
}

const char   *
dang_builder_get_var_name    (DangBuilder *builder,
                                       DangVarId            var_id)
{
  dang_assert (var_id < builder->vars.len);
  return GET_VAR (builder, var_id)->name;
}

void
dang_builder_push_local_scope(DangBuilder *builder)
{
  Scope *scope = dang_new (Scope, 1);
  DANG_UTIL_ARRAY_INIT (&scope->var_ids, DangVarId);
  scope->up = builder->local_scope;
  builder->local_scope = scope;
}

void
dang_builder_push_tmp_scope  (DangBuilder *builder)
{
  Scope *scope = dang_new (Scope, 1);
  DANG_UTIL_ARRAY_INIT (&scope->var_ids, DangVarId);
  scope->up = builder->tmp_scope;
  builder->tmp_scope = scope;
}

typedef struct 
{
  unsigned offset;
  DangValueType *type;
} OneDestructInfo;

void
dang_builder_compile_destructs(DangBuilder *builder,
                                        unsigned n_to_destruct,
                                        DangVarId *to_destruct)
{
  DangInsn insn;
  unsigned i;
  dang_insn_init (&insn, DANG_INSN_TYPE_DESTRUCT);
  for (i = 0; i < n_to_destruct; i++)
    {
      insn.destruct.var = to_destruct[i];
      dang_builder_add_insn (builder, &insn);
    }
}

static void
kill_scope (Builder *builder,
            Scope   *to_kill)
{
  unsigned i;
  DangVarId *ids = to_kill->var_ids.data;
  DangVarId *destruct_ids = dang_new (DangVarId, to_kill->var_ids.len);
  unsigned n_destruct_ids = 0;
  DangStepNum end;
  if (to_kill->var_ids.len > 0)
    {
      for (i = 0; i < to_kill->var_ids.len; i++)
        {
          DangVarId id = ids[i];
          Variable *var = GET_VAR (builder, id);
          if (var->end != DANG_STEP_NUM_INVALID)
            continue;           /* scope already ended */
          if (var->container != DANG_VAR_ID_INVALID)
            continue;
          dang_assert (var->type != NULL);
          if (var->type->destruct != NULL)
            destruct_ids[n_destruct_ids++] = id;
          dang_assert (var->end == DANG_STEP_NUM_INVALID);
        }

      /* Compute the last instruction these variables are used in. */
      if (n_destruct_ids > 0)
        end = builder->insns.len;
      else
        {
          /* There should have been an initialize instruction */
          dang_assert (builder->insns.len > 0);
          end = builder->insns.len - 1;
        }
      for (i = 0; i < to_kill->var_ids.len; i++)
        GET_VAR (builder, ids[i])->end = end;

      /* Add destruct instruction, if needed */
      dang_builder_compile_destructs (builder,
                                               n_destruct_ids,
                                               destruct_ids);
      dang_free (destruct_ids);
    }

  dang_util_array_clear (&to_kill->var_ids);
  dang_free (to_kill);
}

void
dang_builder_pop_local_scope (DangBuilder *builder)
{
  Scope *kill = builder->local_scope;
  dang_assert (kill != NULL);
  builder->local_scope = kill->up;

  kill_scope (builder, kill);
}

void
dang_builder_pop_tmp_scope   (DangBuilder *builder)
{
  Scope *kill = builder->tmp_scope;
  dang_assert (kill != NULL);
  builder->tmp_scope = kill->up;

  kill_scope (builder, kill);
}

void
dang_builder_note_var_create (DangBuilder *builder,
                                            DangVarId            id)
{
  Variable *var = GET_VAR (builder, id);
  dang_assert (var->start == DANG_STEP_NUM_INVALID);
  var->start = builder->insns.len;
}

void
dang_builder_note_var_destruct(DangBuilder *builder,
                                            DangVarId            id)
{
  Variable *var = GET_VAR (builder, id);
  dang_assert (var->end == DANG_STEP_NUM_INVALID);
  var->end = builder->insns.len;
}

DangCatchBlockId
dang_builder_start_catch_block (DangBuilder *builder,
                                         unsigned n_clauses,
                                         DangBuilderCatchClause *clauses)
{
  DangBuilderCatchBlock cb;
  DangCatchBlockId id = builder->catch_blocks.len;
  cb.start = builder->insns.len;
  cb.end = DANG_STEP_NUM_INVALID;
  cb.n_clauses = n_clauses;
  cb.clauses = clauses;
  dang_util_array_append (&builder->catch_blocks, 1, &cb);
  return id;
}

void
dang_builder_end_catch_block (DangBuilder *builder,
                                       DangCatchBlockId     id)
{
  DangBuilderCatchBlock *cb;
  dang_assert (id < builder->catch_blocks.len);
  cb = (DangBuilderCatchBlock*) builder->catch_blocks.data + id;
  dang_assert (cb->end == DANG_STEP_NUM_INVALID);
  cb->end = builder->insns.len;
}

void
dang_builder_query_vars      (DangBuilder *builder,
                                       DangStepNum          source,
                                       DangStepNum          target,
                                       unsigned            *n_destruct_out,
                                       DangVarId          **destruct_out,
                                       unsigned            *n_init_out,
                                       DangVarId          **init_out)
{
  unsigned n_vars = builder->vars.len;
  Variable *vars = builder->vars.data;
  DangVarId id;
  DangArray destruct;
  DangArray init;
  DANG_UTIL_ARRAY_INIT (&destruct, DangVarId);
  DANG_UTIL_ARRAY_INIT (&init, DangVarId);
  for (id = 0; id < n_vars; id++)
    {
      dang_boolean was_live, will_be_live;
      if (vars[id].container != DANG_VAR_ID_INVALID)
        continue;
      was_live     = source != DANG_STEP_NUM_INVALID
                  && vars[id].start <= source
                  && source <= vars[id].end;
      will_be_live = target != DANG_STEP_NUM_INVALID
                  && vars[id].start <= target
                  && target <= vars[id].end;
      if (was_live && !will_be_live)
        {
          if (vars[id].type->destruct != NULL)
            dang_util_array_append (&destruct, 1, &id);
        }
      else if (!was_live && will_be_live)
        dang_util_array_append (&init, 1, &id);
    }

  if (n_destruct_out)
    {
      *n_destruct_out = destruct.len;
      *destruct_out = destruct.data;
    }
  else
    {
      dang_assert (destruct.len == 0);
      dang_util_array_clear (&destruct);
    }
  if (n_init_out)
    {
      *n_init_out = init.len;
      *init_out = init.data;
    }
  else
    {
      dang_assert (init.len == 0);
      dang_util_array_clear (&init);
    }
}

void
dang_builder_push_friend (DangBuilder *builder,
                                   DangValueType *type,
                                   dang_boolean private_access)
{
  DangBuilderFriend *f = dang_new (DangBuilderFriend, 1);
  f->type = type;
  f->private_access = private_access;
  f->next = builder->friend_stack;
  builder->friend_stack = f;
}

static dang_boolean
check_private (DangBuilder *builder,
               DangValueType       *type,
               dang_boolean         requires_private,
               DangError          **error)
{
  DangBuilderFriend *f;
  dang_boolean just_need_private = FALSE;
  for (f = builder->friend_stack; f; f = f->next)
    if (f->type == type)
      {
        if (requires_private && f->private_access)
          break;
        else if (!requires_private)
          break;
        else
          just_need_private = TRUE;
      }
  if (f == NULL)
    {
      if (just_need_private)
        dang_set_error (error, "had protected access, needed private access");
      else
        dang_set_error (error, "had public access, required %s",
                        requires_private ? "private" : "protected");
      return FALSE;
    }
  return TRUE;
}

dang_boolean
dang_builder_check_member_access(DangBuilder *builder,
                                 DangValueType *type,
                                 DangMemberFlags flags,
                                 dang_boolean read_access,
                                 dang_boolean write_access,
                                 DangError   **error)
{
  int level = 0; /* 0 for public, 1 for protected, 2 for private */
  if (read_access)
    {
      if ((flags & DANG_MEMBER_PUBLIC_READABLE) == 0)
        {
          if (flags & DANG_MEMBER_PROTECTED_READABLE)
            level = 1;
          else if (flags & DANG_MEMBER_PRIVATE_READABLE)
            level = 2;
          else
            {
              dang_set_error (error, "member is not readable");
              return FALSE;
            }
        }
    }
  if (write_access)
    {
      if ((flags & DANG_MEMBER_PUBLIC_WRITABLE) == 0)
        {
          if (flags & DANG_MEMBER_PROTECTED_WRITABLE)
            level = DANG_MAX (level, 1);
          else if (flags & DANG_MEMBER_PRIVATE_WRITABLE)
            level = DANG_MAX (level, 2);
          else
            {
              dang_set_error (error, "member is not writable");
              return FALSE;
            }
        }
    }
  if (level == 0)
    return TRUE;
  if (check_private (builder, type, level == 2, error))
    return TRUE;

  if (((flags & DANG_MEMBER_PUBLIC_READABLE) != 0) && write_access)
    dang_error_add_suffix (*error, "member is 'readonly'");
  return FALSE;
}

dang_boolean 
dang_builder_check_method_access(DangBuilder *builder,
                                          DangValueType *type,
                                          DangMethodFlags flags,
                                          DangError   **error)
{
  if (flags & (DANG_METHOD_PUBLIC|DANG_METHOD_PUBLIC_READONLY))
    return TRUE;
  return check_private (builder, type, 
                        (flags & DANG_METHOD_PRIVATE) == DANG_METHOD_PRIVATE,
                        error);
}


static void
free_scope_list (Scope *scope)
{
  while (scope != NULL)
    {
      Scope *up = scope->up;
      dang_util_array_clear (&scope->var_ids);
      dang_free (scope);
      scope = up;
    }
}

/**
 * dang_builder_destroy:
 * @builder: the builder to free.
 *
 * Free memory associated with the builder;
 * this may be called after dang_builder_compile() to clean
 * up normally, or it may be called without calling dang_builder_compile
 * to abort compilation.
 */
void
dang_builder_destroy    (DangBuilder *builder)
{
  unsigned i;
  for (i = 0; i < builder->insns.len; i++)
    dang_insn_destruct (((DangInsn*)builder->insns.data) + i);
  dang_util_array_clear (&builder->insns);

  for (i = 0; i < builder->labels.len; i++)
    {
      Label *label = (Label *) builder->labels.data + i;
      dang_free (label->name);
      dang_code_position_clear (&label->first_goto_position);
      dang_code_position_clear (&label->definition_position);
    }
  dang_util_array_clear (&builder->labels);
  for (i = 0; i < builder->scoped_labels.len; i++)
    {
      ScopedLabel *sl = (ScopedLabel *) builder->scoped_labels.data + i;
      dang_free (sl->name);
      dang_util_array_clear (&sl->labels);
    }
  dang_util_array_clear (&builder->scoped_labels);
  for (i = 0; i < builder->vars.len; i++)
    {
      Variable *var = GET_VAR (builder, i);
      dang_free (var->name);
    }
  dang_util_array_clear (&builder->vars);
  free_scope_list (builder->local_scope);
  free_scope_list (builder->tmp_scope);
  for (i = 0; i < builder->catch_blocks.len; i++)
    {
      CatchBlock *cb = (CatchBlock *) builder->catch_blocks.data + i;
      dang_free (cb->clauses);
    }
  dang_util_array_clear (&builder->catch_blocks);

  if (builder->sig)
    dang_signature_unref (builder->sig);
  if (builder->imports)
    dang_imports_unref (builder->imports);
  dang_code_position_clear (&builder->pos);
  dang_function_unref (builder->function);


  while (builder->friend_stack)
    {
      DangBuilderFriend *f = builder->friend_stack;
      builder->friend_stack = f->next;
      dang_free (f);
    }

  dang_free (builder);
}
