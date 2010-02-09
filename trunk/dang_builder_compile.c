#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "dang.h"
#include "config.h"
#include "gskqsortmacro.h"

typedef DangBuilder         Builder;
typedef DangBuilderVariable Variable;
typedef DangBuilderLabel    Label;

/* Check the builder for consistency and well-definedness */
/* TODO: get all vars were initialized */
static dang_boolean check__labels_targetted (DangBuilder *builder, DangError **);
static dang_boolean check__variables (DangBuilder *builder, DangError **);
static void add_inits_and_destructs (DangBuilder *builder);

/* Allocate the locations of the return-value and
   parameters to this function. */
static void
allocate_stack__rv_and_params (Builder *builder,
                               unsigned *first_offset_out);

/* Allocate all the other variables for this function,
   (except variables which are merely aliases into
   other variebles). */
static void
allocate_stack__first_fit (Builder *builder,
                           unsigned *frame_size_inout);

/* Compute the offsets of the aliases--
   they are just pointers into their container variable. */
static void
allocate_stack__aliases (Builder *builder);

static void
init_stack_infos__file_info (Builder *builder,
                             char *step_data_blob,
                             unsigned *final_step_offsets,
                             unsigned *n_file_out,
                             DangFunctionFileInfo ***file_info_out);

#if DANG_DEBUG
static void dump_insns (Builder *builder);
#endif

/* Function: dang_builder_compile
 * Convert the Stub function used to construct the builder
 * into a real native Dang function.
 * 
 * Parameters:
 *     builder - the function builder object.
 *     error - place for error if something goes wrong
 * Returns:
 *     whether the compilation succeeded
 */
dang_boolean   
dang_builder_compile    (DangBuilder *builder,
				  DangError          **error)
{
  unsigned n_steps;
  DangInsn *steps;
  unsigned i;
  unsigned n_vars;
  Variable *vars;
  unsigned frame_size;
  unsigned at, param_at;
  unsigned n_stack_info_vars;
  unsigned n_stack_info_params;
  DangBuilderCatchBlock *src_catch_blocks;
  Label *labels;
  DangFunctionDang rv;
  DangFunction *old_stub;
  DangInsnPackContext context;
  unsigned *final_step_offsets;

  /* Pop the last tmp-scope; alas, this often results in dead code. */
  dang_builder_pop_tmp_scope (builder);

  if (builder->needs_return)
    dang_builder_add_return (builder);

  if (!check__labels_targetted (builder, error)
   || !check__variables (builder, error))
    return FALSE;

  //fprintf(stderr, "BEFORE INITS AND DESTRUCTS\n"); dump_insns (builder);
  add_inits_and_destructs (builder);
  //fprintf(stderr, "AFTER INITS AND DESTRUCTS\n"); dump_insns (builder);
#if DANG_DEBUG
  if (dang_debug_disassemble)
    dump_insns (builder);
#endif

  allocate_stack__rv_and_params (builder, &frame_size);
  allocate_stack__first_fit (builder, &frame_size);
  allocate_stack__aliases (builder);

  n_steps = builder->insns.len;
  steps = builder->insns.data;
  n_vars = builder->vars.len;
  vars = builder->vars.data;

  /* Pack into DangStep's.  Note fixups, meaning places where places that 
     must be replaced with pointers to DangSteps, once the packed slab is allocated. */
  DANG_UTIL_ARRAY_INIT (&context.step_data, char);
  DANG_UTIL_ARRAY_INIT (&context.label_fixups, DangInsnLabelFixup);
  DANG_UTIL_ARRAY_INIT (&context.destroys, DangInsnDestroy);
  labels = builder->labels.data;
  final_step_offsets = dang_new (unsigned, n_steps);
  context.n_vars = builder->vars.len;
  context.vars = builder->vars.data;
  context.labels = builder->labels.data;
  for (i = 0; i < n_steps; i++)
    {
      final_step_offsets[i] = context.step_data.len;
      dang_insn_pack (steps + i, &context);
    }
#define GET_FINAL_STEP(step_num)      \
            (DangStep *) ((char*)context.step_data.data + final_step_offsets[(step_num)])
  for (i = 0; i < context.label_fixups.len; i++)
    {
      DangInsnLabelFixup *fixup = ((DangInsnLabelFixup*)context.label_fixups.data) + i;
      DangStep **p_step = (DangStep**)((char*)context.step_data.data + fixup->step_data_offset);
      DangStepNum step = labels[fixup->label].target;
      *p_step = GET_FINAL_STEP (step);
    }

  /* Create the stack information data */
  vars = builder->vars.data;
  n_stack_info_vars = 0;
  n_stack_info_params = 0;
  for (i = 0; i < builder->vars.len; i++)
    if (vars[i].is_param)
      n_stack_info_params++;
    else if (vars[i].start != vars[i].end)
      n_stack_info_vars++;

  DangFunctionStackInfo *stack_info;
  stack_info = dang_new (DangFunctionStackInfo, 1);
  stack_info->n_vars = n_stack_info_vars;
  stack_info->vars = dang_new (DangFunctionStackVarInfo, n_stack_info_vars);
  stack_info->n_params = n_stack_info_params;
  stack_info->params = dang_new (DangFunctionStackParamInfo, n_stack_info_params);
  at = 0;
  param_at = 0;
  for (i = 0; i < builder->vars.len; i++)
    if (vars[i].is_param)
      {
        stack_info->params[param_at].offset = vars[i].offset;
        stack_info->params[param_at].type = vars[i].type;
        param_at++;
      }
    else if (vars[i].start != vars[i].end)
      {
        stack_info->vars[at].start = GET_FINAL_STEP (vars[i].start);
        stack_info->vars[at].end = GET_FINAL_STEP (vars[i].end);
        stack_info->vars[at].offset = vars[i].offset;
        stack_info->vars[at].type = vars[i].type;
        at++;
      }
  stack_info->n_catch_blocks = builder->catch_blocks.len;
  stack_info->catch_blocks = dang_new (DangCatchBlock, builder->catch_blocks.len);
  src_catch_blocks = builder->catch_blocks.data;
  for (i = 0; i < builder->catch_blocks.len; i++)
    {
      unsigned j;
      DangCatchBlock *dst = stack_info->catch_blocks + i;
      DangBuilderCatchBlock *src = src_catch_blocks + i;
      dst->start = GET_FINAL_STEP (src->start);
      dst->end = GET_FINAL_STEP (src->end);
      dst->n_clauses = src->n_clauses;
      dst->clauses = dang_new (DangCatchBlockClause, src->n_clauses);
      for (j = 0; j < src->n_clauses; j++)
        {
          Label *label = labels + src->clauses[j].target;
          dst->clauses[j].type = src->clauses[j].type;
          dang_assert (label->target != DANG_STEP_NUM_INVALID);
          dst->clauses[j].catch_target = GET_FINAL_STEP (label->target);
          dst->clauses[j].catch_var_offset = vars[src->clauses[j].var_id].offset;
        }
    }
  stack_info->first_step = GET_FINAL_STEP (0);
  stack_info->last_step = GET_FINAL_STEP (n_steps - 1);
  init_stack_infos__file_info (builder,
                               context.step_data.data, final_step_offsets,
                               &stack_info->n_file_info, &stack_info->file_info);

  ///DangCatchBlock *catch_blocks;
  ///DangStep *first_step, *last_step;
  
  /* Create the dang_function */
  rv.base.type = DANG_FUNCTION_TYPE_DANG;
  rv.base.ref_count = 1;
  rv.base.compile = NULL;
  rv.base.stack_info = stack_info;
  rv.base.sig = dang_signature_ref (builder->sig);
  rv.base.frame_size = frame_size;
  rv.base.steps = stack_info->first_step;
  rv.base.is_owned = builder->function->base.is_owned;
  rv.n_destroy = context.destroys.len;
  rv.destroy = dang_new (DangFunctionDangDestruct, rv.n_destroy);
  for (i = 0; i < context.destroys.len; i++)
    {
      DangInsnDestroy *d = (DangInsnDestroy*)context.destroys.data + i;
      rv.destroy[i].func = d->func;
      if (d->is_step_data_destroy)
        rv.destroy[i].arg1 = (char*)context.step_data.data + d->offset;
      else
        rv.destroy[i].arg1 = d->arg1;
      rv.destroy[i].arg2 = d->arg2;
    }
  old_stub = builder->function;
  dang_builder_destroy (builder);

  dang_free (context.label_fixups.data);
  dang_free (context.destroys.data);
  dang_free (final_step_offsets);
  
  dang_imports_unref (old_stub->stub.imports);
  dang_expr_unref (old_stub->stub.body);
  dang_signature_unref (old_stub->base.sig);
  rv.base.ref_count = old_stub->base.ref_count;
  old_stub->dang = rv;
  return TRUE;
}

/* --- stack space allocation functions --- */
static void
allocate_stack__rv_and_params (Builder *builder,
                               unsigned *first_offset_out)
{
  DangSignature *sig = builder->sig;
  unsigned offset = sizeof (DangThreadStackFrame);
  DangVarId id = 0;
  Variable *vars = builder->vars.data;
  unsigned i;
  if (sig->return_type)
    {
      DangValueType *type = sig->return_type;
      offset += type->alignof_instance - 1;
      offset &= ~(type->alignof_instance - 1);
      vars[id].offset = offset;
      offset += type->sizeof_instance;
      id++;
    }
  for (i = 0; i < sig->n_params; i++)
    {
      DangValueType *type = sig->params[i].type;
      offset += type->alignof_instance - 1;
      offset &= ~(type->alignof_instance - 1);
      vars[id].offset = offset;
      offset += type->sizeof_instance;
      id++;
    }
  *first_offset_out = offset;
}

/* Assign offsets to variables. */
typedef struct 
{
  Variable *var;
  dang_boolean is_start;
  DangStepNum step;
} Action;
typedef struct 
{
  unsigned start, size;
} FreeBlock;

static inline dang_boolean
free_block_accomodates (const FreeBlock *fb,
                        DangValueType   *type,
                        unsigned        *offset_out)
{
  unsigned offset = fb->start + type->alignof_instance - 1;
  offset &= ~(type->alignof_instance - 1);
  offset -= fb->start;
  if (offset + type->sizeof_instance > fb->size)
    return FALSE;
  *offset_out = offset;
  return TRUE;
}
static void
allocate_stack__first_fit (Builder *builder,
                           unsigned *frame_size_inout)
{
  unsigned i;
  unsigned n_vars = builder->vars.len;
  Variable *vars = builder->vars.data;
  Action *actions = dang_new (Action, n_vars * 2);
  unsigned n_actions = 0;
  DangUtilArray free_blocks;
  for (i = 0; i < n_vars; i++)
    {
      /* Disregard variables that are just aliases
         to some other variable. */
      if (vars[i].container != DANG_VAR_ID_INVALID)
        continue;

      /* Disregard the parameters and the return-value,
         because they are already allocated. */
      if (vars[i].offset != 0)
        continue;

      actions[n_actions].var = vars + i;
      actions[n_actions].is_start = TRUE;
      actions[n_actions].step = vars[i].start;
      n_actions++;

      actions[n_actions].var = vars + i;
      actions[n_actions].is_start = FALSE;
      actions[n_actions].step = vars[i].end;
      n_actions++;
    }

  /* Sort the actions by step num */
#define COMPARE(a,b,rv)                  \
  if (a.step < b.step)                   \
    rv = -1;                             \
  else if (a.step > b.step)              \
    rv = 1;                              \
  else if (a.is_start < b.is_start)      \
    rv = 1;                              \
  else if (a.is_start > b.is_start)      \
    rv = -1;                             \
  else                                   \
    rv = 0;
  GSK_QSORT (actions, Action, n_actions, COMPARE);
#undef COMPARE

  DANG_UTIL_ARRAY_INIT (&free_blocks, FreeBlock);

  for (i = 0; i < n_actions; i++)
    if (actions[i].is_start)
      {
        /* Allocate variable */
        FreeBlock *tmp_fbs = free_blocks.data;
        unsigned f;
        unsigned offset;
        dang_boolean did_allocation = FALSE;
        DangValueType *type = actions[i].var->type;
        for (f = 0; f < free_blocks.len; f++)
          if (free_block_accomodates (tmp_fbs, type, &offset))
            {
              unsigned size = type->sizeof_instance;
              dang_boolean at_start = offset == 0;
              dang_boolean at_end = offset + size == tmp_fbs[f].size;
              actions[i].var->offset = tmp_fbs[f].start + offset;
              if (at_start && at_end)
                {
                  /* Free block is destroyed. */
                  dang_util_array_remove (&free_blocks, f, 1);
                }
              else if (at_start)
                {
                  tmp_fbs[f].size -= size;
                  tmp_fbs[f].start += size;
                }
              else if (at_end)
                {
                  tmp_fbs[f].size -= size;
                }
              else
                {
                  FreeBlock new;
                  new.start = tmp_fbs[f].start + offset + size;
                  new.size = tmp_fbs[f].start - offset - size;
                  tmp_fbs[f].size = offset;
                  dang_util_array_insert (&free_blocks, 1, &new, f+1);
                }
              did_allocation = TRUE;
              break;
            }

        if (!did_allocation)
          {
            /* Need to extend the stack. */
            unsigned off = DANG_ALIGN (*frame_size_inout, type->alignof_instance);
            actions[i].var->offset = off;
            *frame_size_inout = off + type->sizeof_instance;
          }
        //dang_warning ("allocate %s: %u", actions[i].var->type->full_name, actions[i].var->offset);
      }
    else
      {
        /* Deallocate space used for variable.
         *
         * Find the nearest free block.
         * The possibilities are 4:
         * - whether the start is adjacent to another free block
         * - whether the end is adjacent to another free block
         */
        FreeBlock *tmp_fbs = free_blocks.data;
        unsigned var_start, var_size, var_end;
        dang_boolean adj_start = FALSE, adj_end = FALSE;
        unsigned f;
        /* Find the first block which comes AFTER
           the piece of stack to liberate. */
        for (f = 0; f < free_blocks.len; f++)
          if (tmp_fbs[f].start > actions[i].var->offset)
            break;
        var_start = actions[i].var->offset;
        var_size = actions[i].var->type->sizeof_instance;
        var_end = var_start + var_size;
        if (f > 0)
          {
            unsigned prev_end = tmp_fbs[f-1].start + tmp_fbs[f-1].size;
            adj_start = prev_end == var_start;
          }
        if (f < free_blocks.len)
          {
            unsigned next_start = tmp_fbs[f].start;
            adj_end = var_end == next_start;
          }
        if (adj_start && adj_end)
          {
            unsigned new_end = tmp_fbs[f].size + tmp_fbs[f].start;
            tmp_fbs[f-1].size = new_end - tmp_fbs[f-1].start;
            dang_util_array_remove (&free_blocks, f, 1);
          }
        else if (adj_start)
          {
            tmp_fbs[f-1].size += var_size;
          }
        else if (adj_end)
          {
            tmp_fbs[f].size += var_size;
            tmp_fbs[f].start -= var_size;
          }
        else
          {
            FreeBlock tmp;
            tmp.start = var_start;
            tmp.size = var_size;
            dang_util_array_insert (&free_blocks, 1, &tmp, f);
          }
        //dang_warning ("deallocate %s: %u", actions[i].var->type->full_name, actions[i].var->offset);
      }

  /* Clean up */
  dang_free (actions);
  dang_util_array_clear (&free_blocks);
}

static void
allocate_stack__aliases (Builder *builder)
{
  unsigned n_vars = builder->vars.len;
  Variable *vars = builder->vars.data;
  DangVarId id;
  for (id = 0; id < n_vars; id++)
    if (vars[id].container != DANG_VAR_ID_INVALID)
      vars[id].offset = vars[vars[id].container].offset
                     + vars[id].container_offset;
}


/* --- init_stack_infos__file_info --- */
typedef struct _Triple Triple;
struct _Triple
{
  DangStep *step;
  DangString *filename;
  unsigned line;
};
static DangFunctionFileInfo *
create_file_info (DangString *filename,
                  DangUtilArray  *line_infos)
{
  DangFunctionFileInfo *fi = dang_malloc (sizeof (DangFunctionFileInfo)
                                          * sizeof (DangFunctionLineInfo) * line_infos->len);
  fi->n_line_infos = line_infos->len;
  fi->filename = dang_string_ref_copy (filename);
  memcpy (&fi->line_infos, line_infos->data, sizeof (DangFunctionLineInfo) * line_infos->len);
  return fi;
}
static void
init_stack_infos__file_info (Builder *builder,
                             char *step_data_blob,
                             unsigned *final_step_offsets,
                             unsigned *n_file_out,
                             DangFunctionFileInfo ***file_info_out)
{
  DangStep *last_step = NULL;
  unsigned n_steps = builder->insns.len;
  DangInsn *steps = builder->insns.data;
  unsigned i;
  DangUtilArray array;
  DangUtilArray line_infos;
  DangString *last_filename = NULL;
  unsigned n_filenames = 0;
  DANG_UTIL_ARRAY_INIT (&array, Triple);
  for (i = 0; i < n_steps; i++)
    {
      if (steps[i].base.cp.filename)
        {
          DangStep *step = (DangStep *)(step_data_blob + final_step_offsets[i]);
          //if (step != last_step)
            {
              Triple triple;
              triple.step = step;
              triple.filename = steps[i].base.cp.filename;
              triple.line = steps[i].base.cp.line;
              if (step == last_step)
                ((Triple*)array.data)[array.len-1] = triple;
              else
                {
                  dang_util_array_append (&array, 1, &triple);
                  last_step = step;
                }
            }
        }
    }
  if (array.len == 0)
    {
      *n_file_out = 0;
      *file_info_out = NULL;
      return;
    }
  last_filename = NULL;
  for (i = 0; i < array.len; i++)
    {
      DangString *f = ((Triple*)array.data)[i].filename;
      if (last_filename == NULL || strcmp (f->str, last_filename->str) != 0)
        {
          n_filenames++;
          last_filename = f;
        }
    }
  *n_file_out = n_filenames;
  *file_info_out = dang_new (DangFunctionFileInfo *, n_filenames);
  last_filename = NULL;
  DANG_UTIL_ARRAY_INIT (&line_infos, DangFunctionLineInfo);
  n_filenames = 0;
  for (i = 0; i < array.len; i++)
    {
      Triple *triple = ((Triple*)array.data) + i;
      if (last_filename == NULL || strcmp (triple->filename->str, last_filename->str) != 0)
        {
          if (line_infos.len > 0)
            /* Allocate FileInfo */
            (*file_info_out)[n_filenames-1] = create_file_info (last_filename, &line_infos);
          n_filenames++;
          last_filename = triple->filename;
          dang_util_array_set_size (&line_infos, 0);
        }
      if (line_infos.len == 0
        || ((DangFunctionLineInfo*)line_infos.data)[line_infos.len-1].line != triple->line)
        {
          DangFunctionLineInfo line_info;
          line_info.step = triple->step;
          line_info.line = triple->line;
          dang_util_array_append (&line_infos, 1, &line_info);
        }
    }
  /* Allocate FileInfo */
  if (line_infos.len > 0)
    (*file_info_out)[n_filenames-1] = create_file_info (last_filename, &line_infos);
  dang_util_array_clear (&array);
  dang_util_array_clear (&line_infos);
}


static dang_boolean
check__labels_targetted (DangBuilder *builder,
                         DangError **error)
{
  unsigned i;
  Label *labels = builder->labels.data;
  for (i = 0; i < builder->labels.len; i++)
    {
      if (labels[i].target == DANG_STEP_NUM_INVALID)
        {
          if (labels[i].name != NULL && labels[i].first_goto_position.filename != NULL)
            dang_set_error (error, "label '%s' was never targetted (first goto "DANG_CP_FORMAT")", labels[i].name,
                            DANG_CP_ARGS (labels[i].first_goto_position));
          else if (labels[i].name != NULL)
            dang_set_error (error, "label '%s' was never targetted", labels[i].name);
          else
            dang_set_error (error, "unnamed label %u was never targetted", i);
          return FALSE;
        }
      if (labels[i].target >= builder->insns.len)
        {
          if (labels[i].name != NULL)
            dang_set_error (error, "label '%s' invalid step", labels[i].name);
          else
            dang_set_error (error, "unnamed label %u invalid step", i);
          return FALSE;
        }
    }
  return TRUE;
}

static dang_boolean
check__variables (DangBuilder *builder,
                  DangError **error)
{
  unsigned i;
  Variable *vars = builder->vars.data;
  for (i = 0; i < builder->vars.len; i++)
    {
      if (vars[i].type == NULL)
        {
          if (vars[i].name == NULL)
            dang_set_error (error, "variable '%s' was never typed", vars[i].name);
          else
            dang_set_error (error, "unnamed variable #%u was never typed", i);
          return FALSE;
        }
      if (!vars[i].bound)
        {
          dang_set_error (error, "variable #%u was never bound", i);
          return FALSE;
        }
      if (!vars[i].is_param && vars[i].container == DANG_VAR_ID_INVALID)
        {
          if (vars[i].start == DANG_STEP_NUM_INVALID)
            {
              dang_set_error (error, "variable #%u has no start step", i);
              return FALSE;
            }
          if (vars[i].end == DANG_STEP_NUM_INVALID)
            {
              dang_set_error (error, "variable #%u has no end step", i);
              return FALSE;
            }
        }
    }
  return TRUE;
}

/* Modify JUMP, JUMP_CONDITIONAL, RETURN
   to add INIT and DESTRUCT operations as needed.

   Fix up Variables and Labels. */
typedef struct _VarFixupInsertion VarFixupInsertion;
struct _VarFixupInsertion
{
  DangStepNum orig_step_num;
  unsigned n_add;
  DangInsn *added_insns;
  unsigned added_thus_far;
};

static void
renumber_step  (DangStepNum              *stepnum_inout,
                unsigned                  n_fixups,
                VarFixupInsertion        *fixups)
{
  /* Find the fixup that immediately strictly precedes *stepnum_inout;
     if none, then no change is required */
  unsigned start = 0, n = n_fixups;
  while (n > 1)
    {
      unsigned mid = start + n / 2;
      if (fixups[mid].orig_step_num >= *stepnum_inout)
        {
          n /= 2;
        }
      else
        {
          n = start + n - mid;
          start = mid;
        }
    }
  if (n == 0)
    return;
  if (fixups[start].orig_step_num >= *stepnum_inout)
    {
      if (start == 0)
        return;
      else
        *stepnum_inout += fixups[start].added_thus_far;
    }
  else
    *stepnum_inout += fixups[start].added_thus_far + fixups[start].n_add;
}

static void
renumber_label (Label *label,
                unsigned                  n_fixups,
                VarFixupInsertion        *fixups)
{
  renumber_step (&label->target, n_fixups, fixups);

  /* XXX: unnecessary... */
  if (label->type == DANG_FUNCTION_BUILDER_LABEL_TYPE_SCOPED)
    {
      renumber_step (&label->first_active, n_fixups, fixups);
      renumber_step (&label->last_active, n_fixups, fixups);
    }
}
static void
renumber_var   (DangBuilderVariable   *var,
                unsigned                  n_fixups,
                VarFixupInsertion        *fixups)
{
  if (!var->is_param)
    {
      renumber_step (&var->start, n_fixups, fixups);
      renumber_step (&var->end, n_fixups, fixups);
    }
}
static void add_inits_and_destructs (DangBuilder *builder)
{
  DangUtilArray fixups = DANG_UTIL_ARRAY_STATIC_INIT (VarFixupInsertion);
  unsigned i, j;
  DangInsn *orig_insns = builder->insns.data;
  unsigned n_init, n_destruct;
  DangVarId *init, *destruct;
  Label *labels = builder->labels.data;
  VarFixupInsertion fixup;
  for (i = 0; i < builder->insns.len; i++)
    {
      switch (orig_insns[i].type)
        {
        case DANG_INSN_TYPE_JUMP:
          dang_builder_query_vars (builder, i, labels[orig_insns[i].jump.target].target,
                                            &n_destruct, &destruct, &n_init, &init);
          fixup.orig_step_num = i;
          fixup.n_add = n_destruct + n_init;
          fixup.added_insns = dang_new (DangInsn, fixup.n_add);
          for (j = 0; j < n_destruct; j++)
            {
              dang_insn_init (fixup.added_insns + j, DANG_INSN_TYPE_DESTRUCT);
              dang_code_position_copy (&fixup.added_insns[j].base.cp, &orig_insns[j].base.cp);
              fixup.added_insns[j].destruct.var = destruct[j];
            }
          for (j = 0; j < n_init; j++)
            {
              dang_insn_init (fixup.added_insns + j + n_destruct, DANG_INSN_TYPE_INIT);
              dang_code_position_copy (&fixup.added_insns[j+n_destruct].base.cp, &orig_insns[j].base.cp);
              fixup.added_insns[n_destruct + j].init.var = init[j];
            }
          if (fixup.n_add > 0)
            dang_util_array_append (&fixups, 1, &fixup);
          dang_free (destruct);
          dang_free (init);
          break;

        case DANG_INSN_TYPE_JUMP_CONDITIONAL:
          dang_builder_query_vars (builder, i, labels[orig_insns[i].jump_conditional.target].target,
                                            &n_destruct, &destruct, &n_init, &init);
          if (n_destruct != 0 || n_init != 0)
            {
              DangInsn *added_insns = dang_new (DangInsn, 1 + n_destruct + n_init);
              DangInsn orig_insn = orig_insns[i];

              /* Convert the original conditional jump to an unconditional one */
              orig_insns[i].type = DANG_INSN_TYPE_JUMP;
              orig_insns[i].jump.target = orig_insn.jump_conditional.target;

              /* Prepend an inverted jump that goes to the next insn */
              DangLabelId new_label;
              new_label = dang_builder_create_label_at (builder, i + 1);
              labels = builder->labels.data; /* reload- it may have moved */
              added_insns[0] = orig_insn;
              added_insns[0].jump_conditional.jump_if_zero = !added_insns[0].jump_conditional.jump_if_zero;
              added_insns[0].jump_conditional.target = new_label;
              for (j = 0; j < n_destruct; j++)
                {
                  dang_insn_init (added_insns + j + 1, DANG_INSN_TYPE_DESTRUCT);
                  dang_code_position_copy (&added_insns[j + 1].base.cp, &orig_insn.base.cp);
                  added_insns[j].destruct.var = destruct[j];
                }
              for (j = 0; j < n_init; j++)
                {
                  dang_insn_init (added_insns + j + n_destruct + 1, DANG_INSN_TYPE_INIT);
                  dang_code_position_copy (&added_insns[j+n_destruct].base.cp, &orig_insn.base.cp);
                  added_insns[n_destruct + j + 1].init.var = init[j];
                }

              /* add fixup */
              fixup.orig_step_num = i;
              fixup.n_add = 1 + n_destruct + n_init;
              fixup.added_insns = added_insns;
              dang_util_array_append (&fixups, 1, &fixup);
              dang_free (destruct);
              dang_free (init);
            }
          break;

        case DANG_INSN_TYPE_RETURN:
          dang_builder_query_vars (builder, i, DANG_STEP_NUM_INVALID,
                                            &n_destruct, &destruct, NULL, NULL);
          if (n_destruct > 0)
            {
              fixup.orig_step_num = i;
              fixup.n_add = n_destruct;
              fixup.added_insns = dang_new (DangInsn, n_destruct);
              for (j = 0; j < n_destruct; j++)
                {
                  dang_insn_init (&fixup.added_insns[j], DANG_INSN_TYPE_DESTRUCT);
                  fixup.added_insns[j].destruct.var = destruct[j];
                }
              dang_util_array_append (&fixups, 1, &fixup);
              dang_free (destruct);
            }
          break;

        default:
          break;
        }
    }

  unsigned added_thus_far = 0;
  for (j = 0; j < fixups.len; j++)
    {
      VarFixupInsertion *fixup = (VarFixupInsertion*)fixups.data + j;
      fixup->added_thus_far = added_thus_far;
      added_thus_far += fixup->n_add;
    }

  /* Restructure the insn array */
  unsigned orig_len = builder->insns.len;
  dang_util_array_set_size (&builder->insns, builder->insns.len + added_thus_far);
  for (j = 0; j < fixups.len; j++)
    {
      VarFixupInsertion *fixup = (VarFixupInsertion*)fixups.data + (fixups.len - 1 - j);
      unsigned last = (j == 0) ? orig_len : fixup[1].orig_step_num;
      unsigned n_move = last - fixup[0].orig_step_num;
      unsigned delta = fixup->added_thus_far + fixup->n_add;

      /* shift insn */
      memmove ((DangInsn*)builder->insns.data + fixup->orig_step_num + delta,
               (DangInsn*)builder->insns.data + fixup->orig_step_num,
               n_move * sizeof (DangInsn));

      /* insert new insns */
      memcpy ((DangInsn*)builder->insns.data + fixup->orig_step_num + fixup->added_thus_far,
              fixup->added_insns,
              fixup->n_add * sizeof (DangInsn));
    }

  /* Renumber the labels */
  for (i = 0; i < builder->labels.len; i++)
    renumber_label ((Label*)builder->labels.data + i,
                    fixups.len, fixups.data);

  /* Renumber the variable liveness information */
  for (i = 0; i < builder->vars.len; i++)
    renumber_var ((DangBuilderVariable*)builder->vars.data + i,
                    fixups.len, fixups.data);
  for (j = 0; j < fixups.len; j++)
    {
      VarFixupInsertion *fixup = (VarFixupInsertion*)fixups.data + (fixups.len - 1 - j);
      dang_free (fixup->added_insns);
    }
  dang_free (fixups.data);
}


/* === dump_insns === */
#if DANG_DEBUG
typedef struct { DangLabelId label; DangStepNum step; } LabelStepPair;
static int compare_lab_step_pair_by_step_then_label (const void *a,
                                                     const void *b)
{
  const LabelStepPair *la = a;
  const LabelStepPair *lb = b;
  if (la->step < lb->step) return -1;
  if (la->step > lb->step) return 1;
  if (la->label < lb->label) return -1;
  if (la->label > lb->label) return 1;
  return 0;
}
static void
add_label_step_pair (Builder *builder,
                     DangUtilArray *lab_step_pairs,
                     DangLabelId label)
{
  Label *lab = (Label*)builder->labels.data + label;
  LabelStepPair pair = { label, lab->target };
  dang_util_array_append (lab_step_pairs, 1, &pair);
}

static void
dump_insns (Builder *builder)
{
  DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
  DangUtilArray lab_step_pairs = DANG_UTIL_ARRAY_STATIC_INIT (LabelStepPair);
  unsigned i;
  unsigned n_steps = builder->insns.len;
  DangInsn *steps = builder->insns.data;
  Label *labels = builder->labels.data;
  for (i = 0; i < n_steps; i++)
    if (steps[i].type == DANG_INSN_TYPE_JUMP)
      add_label_step_pair (builder, &lab_step_pairs, steps[i].jump.target);
    else if (steps[i].type == DANG_INSN_TYPE_JUMP_CONDITIONAL)
      add_label_step_pair (builder, &lab_step_pairs, steps[i].jump_conditional.target);
  if (lab_step_pairs.len > 0)
    {
      LabelStepPair *arr;
      unsigned n_out, in_index;
      qsort (lab_step_pairs.data, lab_step_pairs.len,
             sizeof (LabelStepPair), compare_lab_step_pair_by_step_then_label);

      arr = lab_step_pairs.data;
      n_out = 1;
      in_index = 1;
      while (in_index < lab_step_pairs.len)
        {
          if (arr[in_index].label != arr[n_out-1].label)
            arr[n_out++] = arr[in_index];
          in_index++;
        }
      dang_util_array_set_size (&lab_step_pairs, n_out);
    }

  LabelStepPair *pairs = lab_step_pairs.data;
  unsigned n_pairs = lab_step_pairs.len;
  unsigned pairs_at = 0;
  for (i = 0; i < n_steps; i++)
    {
      while (pairs_at < n_pairs && pairs[pairs_at].step == i)
        {
          if (labels[pairs[pairs_at].label].name)
            dang_string_buffer_printf (&buf, "%s:\n", 
                                       labels[pairs[pairs_at].label].name);
          else
            dang_string_buffer_printf (&buf, "LABEL$%u:\n",
                                       pairs[pairs_at].label);
          pairs_at++;
        }
      dang_insn_dump (steps + i, builder->vars.data, builder->labels.data, &buf);
    }
  fprintf (stderr, "Compiled code: [%p]\n%s\n", builder->function, buf.str);
  dang_free (buf.str);
}
#endif
