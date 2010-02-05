#include <string.h>
#include "dang.h"
#include "magic.h"

/* Handlers that are re-used by a number of difference metafunctions. */
static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(dang_mf_annotate__push_local_scope);
static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(dang_mf_annotate__none);
static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(dang_mf_annotate__recurse);


#define dang_metafunction_generic_scoped_label_jump_syntax_check "|(|I)"
DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(dang_mf_compile__generic_scoped_label_jump);


#define DANG_METAFUNCTION_INIT(shortname)   \
  {                                         \
     "$" #shortname,                        \
     syntax_check__##shortname,             \
     annotate__##shortname,                 \
     compile__##shortname                   \
  }

#define DANG_METAFUNCTION_INIT__SYNTAX_CHECK_ONLY(shortname)   \
  {                                         \
     "$" #shortname,                        \
     syntax_check__##shortname,             \
     NULL,                                  \
     NULL                                   \
  }

#define DANG_BUILTIN_METAFUNCTION(shortname) \
  static DangMetafunction _dang_metafunction__##shortname = DANG_METAFUNCTION_INIT(shortname)
#define DANG_BUILTIN_METAFUNCTION__SYNTAX_CHECK_ONLY(shortname) \
  static DangMetafunction _dang_metafunction__##shortname = DANG_METAFUNCTION_INIT__SYNTAX_CHECK_ONLY(shortname)


/* --- Helpers for the Annotation Phase --- */

void
dang_mf_annotate_value (DangAnnotations *annotations,
                        DangExpr *expr,
                        DangValueType *type,
                        dang_boolean is_lvalue,
                        dang_boolean is_rvalue)
{
  DangExprTag *tag = dang_new (DangExprTag, 1);
  dang_assert (is_lvalue || is_rvalue);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_TAG, tag);
  if (type)
    dang_assert(type->magic == DANG_VALUE_TYPE_MAGIC);
  tag->tag_type = DANG_EXPR_TAG_VALUE;
  tag->info.value.type = type;
  tag->info.value.is_lvalue = is_lvalue;
  tag->info.value.is_rvalue = is_rvalue;
}

void
dang_mf_annotate_type (DangAnnotations *annotations,
                       DangExpr *expr,
                       DangValueType *type)
{
  DangExprTag *tag = dang_new (DangExprTag, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_TAG, tag);
  tag->tag_type = DANG_EXPR_TAG_TYPE;
  tag->info.type = type;
}
void
dang_mf_annotate_function_family (DangAnnotations *annotations,
                                    DangExpr *expr,
                                    DangFunctionFamily *ff)
{
  DangExprTag *tag = dang_new (DangExprTag, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_TAG, tag);
  tag->tag_type = DANG_EXPR_TAG_FUNCTION_FAMILY;
  tag->info.ff.family = ff;
  tag->info.ff.function = NULL;
}
void
dang_mf_annotate_untyped_function (DangAnnotations *annotations,
                                   DangExpr *expr,
                                   DangUntypedFunction *uf)
{
  DangExprTag *tag = dang_new (DangExprTag, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_TAG, tag);
  tag->tag_type = DANG_EXPR_TAG_UNTYPED_FUNCTION;
  tag->info.untyped_function = uf;
}

void
dang_mf_annotate_ns (DangAnnotations *annotations,
                     DangExpr        *expr,
                     DangNamespace   *ns)
{
  DangExprTag *tag = dang_new (DangExprTag, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_TAG, tag);
  tag->tag_type = DANG_EXPR_TAG_NAMESPACE;
  tag->info.ns = ns;
}

void
dang_mf_annotate_statement (DangAnnotations *annotations,
                          DangExpr        *expr)
{
  DangExprTag *tag = dang_new (DangExprTag, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_TAG, tag);
  tag->tag_type = DANG_EXPR_TAG_STATEMENT;
}
void
dang_mf_annotate_method (DangAnnotations *annotations,
                         DangExpr *expr,
                         dang_boolean had_object,
                         DangValueType *object_type,
                         DangValueElement *method_set)
{
  DangExprTag *tag = dang_new (DangExprTag, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_TAG, tag);
  tag->tag_type = DANG_EXPR_TAG_METHOD;
  tag->info.method.object_type = object_type;
  tag->info.method.name = method_set->name;
  tag->info.method.has_object = had_object;
  tag->info.method.method_type = NULL;
  tag->info.method.method_element = NULL;
}
void
dang_mf_annotate_member (DangAnnotations *annotations,
                         DangExpr        *expr,
                         DangValueMember *member)
{
  DangExprMember *tag = dang_new (DangExprMember, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_MEMBER, tag);
  tag->dereference = member->type == DANG_VALUE_MEMBER_TYPE_SIMPLE
                  && member->info.simple.dereference;
}
void
dang_mf_annotate_index_info   (DangAnnotations *annotations,
                               DangExpr        *expr,
                               DangValueIndexInfo *ii)
{
  DangExprIndexInfo *tag = dang_new (DangExprIndexInfo, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_INDEX_INFO, tag);
  tag->index_info = ii;
}

void
dang_mf_annotate_from_namespace_symbol (DangAnnotations     *annotations,
                                      DangExpr            *expr,
                                      DangNamespace       *ns,
                                      DangNamespaceSymbol *symbol)
{
  DangExprNamespaceSymbol *nsym = dang_new (DangExprNamespaceSymbol, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_NAMESPACE_SYMBOL, nsym);
  nsym->ns = ns;
  nsym->symbol = symbol;
  switch (symbol->type)
    {
    case DANG_NAMESPACE_SYMBOL_FUNCTIONS:
      dang_mf_annotate_function_family (annotations, expr, symbol->info.functions);
      break;
    case DANG_NAMESPACE_SYMBOL_GLOBAL:
      dang_mf_annotate_value (annotations, expr, symbol->info.global.type,
                              !symbol->info.global.is_constant, TRUE);
      break;
    case DANG_NAMESPACE_SYMBOL_NAMESPACE:
      dang_mf_annotate_ns (annotations, expr, symbol->info.ns);
      break;
    case DANG_NAMESPACE_SYMBOL_TYPE:
      dang_mf_annotate_type (annotations, expr, symbol->info.type.type);
      break;
    }
}

void
dang_mf_annotate_local_var_id (DangAnnotations *annotations,
                               DangExpr        *expr,
                               DangVarId        var_id)
{
  DangExprVarId *tag = dang_new (DangExprVarId, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_VAR_ID, tag);
  tag->var_id = var_id;
}

DangExprTag *
dang_mf_get_tag (DangAnnotations *annotations,
                 DangExpr        *expr,
                 DangVarTable    *var_table)
{
  DangExprTag *tag;
  DangExprVarId *var_id;
  tag = dang_expr_get_annotation (annotations, expr, DANG_EXPR_ANNOTATION_TAG);
  dang_assert (tag != NULL);
  if (tag->info.value.type != NULL)
    return tag;
  var_id = dang_expr_get_annotation (annotations, expr, DANG_EXPR_ANNOTATION_VAR_ID);
  if (var_id != NULL)
    {
      tag->info.value.type = dang_var_table_get_type (var_table, var_id->var_id);
      if (tag->info.value.type)
        return tag;
    }
  //dang_set_error (error, "needed type for value in '.' (declared at %s:%u)", DANG_CP_EXPR_ARGS (expr));
  return tag;
}


DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(dang_mf_annotate__push_local_scope)
{
  unsigned i;
  dang_var_table_push (var_table);
  for (i = 0; i < expr->function.n_args; i++)
    if (!dang_expr_annotate_types (annotations, expr->function.args[i], imports, var_table, error))
      return FALSE;
  dang_var_table_pop (var_table);
  dang_mf_annotate_statement (annotations, expr);
  return TRUE;
}

static DangInsnValue *
copy_values (unsigned n, DangInsnValue *values)
{
  DangInsnValue *rv = dang_new (DangInsnValue, n);
  unsigned i;
  for (i = 0; i < n; i++)
    dang_insn_value_copy (rv + i, values + i);
  return rv;
}

#define IS_VAR_USED(bytes, which)    \
  ((bytes)[(which)/8] & (1<<((which)%8)))
#define SET_VAR_USED(bytes, which)    \
  ((bytes)[(which)/8] |= (1<<((which)%8)))

/* TODO: refactor to compute array of (unique) VarIds here (build speed) */
static void
gather_closure_params (unsigned n_params,
                       char **param_names,
                       DangVarTable *table,
                       DangExpr *at,
                       uint8_t *vars_used,
                       DangUtilArray *var_ids_used)
{
  unsigned i;
  if (at->type == DANG_EXPR_TYPE_BAREWORD)
    {
      DangVarId var_id;
      DangValueType *type;
      for (i = 0; i < n_params; i++)
        if (strcmp (param_names[i], at->bareword.name) == 0)
          return;
      if (dang_var_table_lookup (table, at->bareword.name, &var_id, &type))
        {
          SET_VAR_USED (vars_used, var_id);
          dang_util_array_append (var_ids_used, 1, &var_id);
          return;
        }
    }
  else if (at->type == DANG_EXPR_TYPE_FUNCTION)
    {
      if (dang_expr_is_function (at, "$operator_dot"))
        {
          gather_closure_params (n_params, param_names, table, at->function.args[0], vars_used, var_ids_used);
        }
      else
        {
          for (i = 0; i < at->function.n_args; i++)
            gather_closure_params (n_params, param_names, table, at->function.args[i], vars_used, var_ids_used);
        }
    }
}

void
dang_mf_gather_closure_params (unsigned n_params,
                               char **param_names,
                               DangVarTable *var_table,
                               DangExpr *at,
                               unsigned *n_var_ids_out,
                               DangVarId **var_ids_out)
{
  DangUtilArray var_ids = DANG_UTIL_ARRAY_STATIC_INIT (DangVarId);
  unsigned vars_used_size = (var_table->variables.len + 7) / 8;
  uint8_t *vars_used = dang_alloca (vars_used_size);
  memset (vars_used, 0, vars_used_size);

  gather_closure_params (n_params, param_names, var_table, at, vars_used, &var_ids);

  *n_var_ids_out = var_ids.len;
  *var_ids_out = var_ids.data;
}


DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(dang_mf_annotate__none)
{
  DANG_UNUSED (annotations);
  DANG_UNUSED (expr);
  DANG_UNUSED (var_table);
  DANG_UNUSED (imports);
  DANG_UNUSED (error);
  return TRUE;
}

DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(dang_mf_annotate__recurse)
{
  unsigned i;
  for (i = 0; i < expr->function.n_args; i++)
    if (!dang_expr_annotate_types (annotations, expr->function.args[i], imports, var_table, error))
      return FALSE;
  return TRUE;
}


/* --- Helpers for the Compile Phase --- */

/* Implement $break(), $continue(), $redo() */
DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(dang_mf_compile__generic_scoped_label_jump)
{
  DangLabelId id;
  const char *name = expr->function.name;
  unsigned level;
  DANG_UNUSED (flags);
  if (expr->function.n_args == 0)
    level = 0;
  else
    {
      level = *(uint32_t*)expr->function.args[0]->value.value;
      if (level == 0)
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "%s 0 not allowed", name+1);
          return;
        }
      level--;
    }

  id = dang_builder_find_scoped_label (builder, name, level);
  if (id == DANG_LABEL_ID_INVALID)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "cannot find label %s", name);
      return;
    }
  dang_builder_add_jump (builder, id);
  dang_compile_result_init_void (result);
}

/* === mf-assign.c === */

/* $assign(lvalue, rvalue) */
#define syntax_check__assign    "AA"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__assign)
{
  DangExprTag *lhs_tag, *rhs_tag;
  if (!dang_expr_annotate_types (annotations, expr->function.args[0], imports, var_table, error)
   || !dang_expr_annotate_types (annotations, expr->function.args[1], imports, var_table, error))
    return FALSE;
  lhs_tag = dang_mf_get_tag (annotations, expr->function.args[0], var_table);
  dang_assert (lhs_tag);
  rhs_tag = dang_mf_get_tag (annotations, expr->function.args[1], var_table);
  dang_assert (rhs_tag);

  if (lhs_tag->tag_type != DANG_EXPR_TAG_VALUE)
    {
      dang_set_error (error, "unexpected tag as first arg to '$assign' (got %s) (%s:%u)",
                      dang_expr_tag_type_name (lhs_tag->tag_type),
                      DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }
  if (rhs_tag->tag_type != DANG_EXPR_TAG_VALUE)
    {
      dang_set_error (error, "unexpected tag as first arg to '$assign' (got %s) (%s:%u)",
                      dang_expr_tag_type_name (rhs_tag->tag_type),
                      DANG_CP_EXPR_ARGS (expr->function.args[1]));
      return FALSE;
    }
  if (!lhs_tag->info.value.is_lvalue)
    {
      dang_set_error (error, "left-hand side of '=' not an lvalue ("DANG_CP_FORMAT")",
                      DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }
  if (!rhs_tag->info.value.is_rvalue)
    {
      dang_set_error (error, "right-hand side of '=' not an rvalue ("DANG_CP_FORMAT")",
                      DANG_CP_EXPR_ARGS (expr->function.args[1]));
      return FALSE;
    }

  if (lhs_tag->info.value.type == NULL
   && rhs_tag->info.value.type != NULL)
    {
      DangExprVarId *var_id = dang_expr_get_annotation (annotations, expr->function.args[0], DANG_EXPR_ANNOTATION_VAR_ID);
      DangValueType *type = dang_var_table_get_type (var_table, var_id->var_id);
      dang_assert (var_id != NULL);
      if (type == NULL)
        {
          dang_var_table_set_type (var_table, var_id->var_id, expr,
                                              rhs_tag->info.value.type);
        }
      else if (type != rhs_tag->info.value.type)
        {
          dang_set_error (error, "type mismatch %s versus %s (from %s:%u)",
                          type->full_name, 
                          rhs_tag->info.value.type->full_name,
                          DANG_CP_EXPR_ARGS (expr));
          return FALSE;
        }
      lhs_tag->info.value.type = rhs_tag->info.value.type;
    }

  dang_mf_annotate_value (annotations, expr, lhs_tag->info.value.type, FALSE, TRUE);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__assign)
{
  DangCompileFlags rvalue_flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  DangCompileResult rvalue_result;
  DangCompileFlags lvalue_flags = DANG_COMPILE_FLAGS_LVALUE_PERMISSIVE;
  DangCompileResult lvalue_result;
  DANG_UNUSED (flags);
  rvalue_flags.permit_global = 1;
  rvalue_flags.permit_pointer = 1;
  rvalue_flags.permit_literal = 1;
  dang_compile (expr->function.args[1],
                builder,
                &rvalue_flags,
                &rvalue_result);
  if (rvalue_result.type == DANG_COMPILE_RESULT_ERROR)
    {
      *result = rvalue_result;
      return;
    }

  dang_compile (expr->function.args[0],
                builder,
                &lvalue_flags,
                &lvalue_result);
  if (lvalue_result.type == DANG_COMPILE_RESULT_ERROR)
    {
      *result = lvalue_result;
      return;
    }

  dang_builder_add_assign (builder, &lvalue_result, &rvalue_result);

  dang_compile_result_clear (&rvalue_result, builder);
  dang_compile_result_clear (&lvalue_result, builder);
  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(assign);
/* === mf-break.c === */

#define syntax_check__break   "|I"
#define annotate__break dang_mf_annotate__none
#define compile__break dang_mf_compile__generic_scoped_label_jump

DANG_BUILTIN_METAFUNCTION(break);
/* === mf-cast.c === */

#define syntax_check__cast   "TA"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__cast)
{
  unsigned i;
  DangExprTag *tag;
  for (i = 0; i < expr->function.n_args; i++)
    if (!dang_expr_annotate_types (annotations, expr->function.args[i], imports, var_table, error))
      return FALSE;
  tag = dang_expr_get_annotation (annotations, expr->function.args[0], DANG_EXPR_ANNOTATION_TAG);
  if (tag->tag_type != DANG_EXPR_TAG_TYPE)
    {
      dang_set_error (error, "first argument to $cast should be a type");
      return FALSE;
    }
  dang_mf_annotate_value (annotations, expr, tag->info.type, FALSE, TRUE);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__cast)
{
  DangValueType *type;
  DangError *error = NULL;
  DangCompileResult subres;
  DangCompileFlags subflags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  DangMatchQuery query;
  DangMatchQueryElement element;
  DangFunction *func;
  DANG_UNUSED (flags);
  type = * (DangValueType**) expr->function.args[0]->value.value;
  if (type->cast_func_name == NULL
   && type->get_cast_func == NULL)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "cannot cast to type %s",
                                     type->cast_func_name);
      return;
    }

  /* TODO: maybe push_tmp_scope() ? */


  subflags.permit_void = 0;
  dang_compile (expr->function.args[1], builder, &subflags, &subres);
  if (subres.type == DANG_COMPILE_RESULT_ERROR)
    {
      *result = subres;
      return;
    }

  if (type->get_cast_func != NULL)
    {
      func = type->get_cast_func (type, subres.any.return_type);
    }
  else
    {
      query.n_elements = 1;
      query.elements = &element;
      query.imports = builder->imports;
      element.type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
      element.info.simple_input = subres.any.return_type;
      func = dang_imports_lookup_function (builder->imports,
                                          1, (char **) &type->cast_func_name,
                                          &query, &error);
    }
  if (func == NULL)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "cast from %s to %s not allowed",
                                     subres.any.return_type->full_name,
                                     type->full_name);
      return;
    }

  DangVarId var_id;
  DangCompileResult lvalue;
  var_id = dang_builder_add_tmp (builder, type);
  dang_compile_result_init_stack (&lvalue, type, var_id, FALSE, TRUE, FALSE);
  dang_compile_literal_function_invocation (func, builder, &lvalue, 1, &subres);
  dang_compile_result_init_stack (result, type, var_id, TRUE, FALSE, TRUE);
  dang_compile_result_clear (&subres, builder);
  dang_compile_result_clear (&lvalue, builder);
  dang_function_unref (func);
}

DANG_BUILTIN_METAFUNCTION(cast);
/* === mf-catch.c === */

#define syntax_check__catch "A$catch_block()*$catch_block()"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__catch)
{
  unsigned i;
  dang_var_table_push (var_table);
  if (!dang_expr_annotate_types (annotations, expr->function.args[0], imports, var_table, error))
    return FALSE;
  dang_var_table_pop (var_table);
  for (i = 1; i < expr->function.n_args; i++)
    {
      DangExpr *sub = expr->function.args[i];
      if (!dang_expr_is_function (sub, "$catch_block"))
        {
          dang_set_error (error, "each argument to $catch after the first should be $catch_block");
          return FALSE;
        }
      dang_var_table_push (var_table);

      if (sub->function.n_args == 3)
        {
          DangVarId id;

          id = dang_var_table_alloc_local (var_table, sub->function.args[1]->bareword.name, sub,
                                           *(DangValueType**)sub->function.args[0]->value.value);
          dang_mf_annotate_local_var_id (annotations, sub->function.args[1], id);
        }
      if (!dang_expr_annotate_types (annotations, sub->function.args[sub->function.n_args-1], imports,
                                     var_table, error))
        return FALSE;

      dang_var_table_pop (var_table);
    }
  dang_mf_annotate_statement (annotations, expr);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE (compile__catch)
{
  /* $catch(statement, $catch_block(...), $catch_block(...)) */
  unsigned i;
  unsigned N = expr->function.n_args - 1;
  DangValueType **types;
  DangExpr **cb_exprs;
  DangCatchBlockId cb_id;
  DangInsn insn;
  DangCompileFlags void_flags = DANG_COMPILE_FLAGS_VOID;
  DangLabelId post_label;
  DangBuilderCatchClause *clauses;

  DANG_UNUSED(flags);

  for (i = 1; i < expr->function.n_args; i++)
    {
      unsigned n_catch_block_args;
      dang_boolean ok;
      n_catch_block_args = expr->function.args[i]->function.n_args;
      /* Catch block must have 3 arguments, unless it is the last catch block,
         which is allowed to have a single catch block. */
      ok = (n_catch_block_args == 3)
        || (i+1 == expr->function.n_args && n_catch_block_args == 1);
      if (!ok)
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "only $catch_block() allowed as non-first arg to $catch()");
          return;
        }
    }

  /* get types */
  types = dang_newa (DangValueType *, N);
  cb_exprs = expr->function.args + 1;
  for (i = 0; i < N; i++)
    {
      if (cb_exprs[i]->function.n_args == 1)
        types[i] = NULL;
      else
        {
          dang_assert (cb_exprs[i]->function.args[0]->type == DANG_EXPR_TYPE_VALUE);
          dang_assert (cb_exprs[i]->function.args[0]->value.type == dang_value_type_type ());
          types[i] = * (DangValueType**) cb_exprs[i]->function.args[0]->value.value;
        }
    }

  /* push a catch block */
  clauses = dang_new (DangBuilderCatchClause, N);
  for (i = 0; i < N; i++)
    {
      clauses[i].type = types[i];
      clauses[i].target = dang_builder_create_label (builder);
      if (types[i] == NULL)
        clauses[i].var_id = DANG_VAR_ID_INVALID;
      else
        {
          DangExprVarId *var_id = dang_expr_get_annotation (builder->annotations,
                                                            cb_exprs[i]->function.args[1],
                                                       DANG_EXPR_ANNOTATION_VAR_ID);
          clauses[i].var_id = var_id->var_id;
        }
    }
  dang_insn_init (&insn, DANG_INSN_TYPE_PUSH_CATCH_GUARD);
  cb_id = dang_builder_start_catch_block (builder, N, clauses);
  insn.push_catch_guard.catch_block_index = cb_id;
  dang_builder_add_insn (builder, &insn);

  /* compile "try" body */
  dang_builder_push_local_scope (builder);
  dang_compile (expr->function.args[0], builder, &void_flags, result);
  dang_builder_pop_local_scope (builder);
  if (result->type == DANG_COMPILE_RESULT_ERROR)
    return;
  dang_compile_result_clear (result, builder);

  /* pop the catch block */
  dang_insn_init (&insn, DANG_INSN_TYPE_POP_CATCH_GUARD);
  dang_builder_end_catch_block (builder, cb_id);
  dang_builder_add_insn (builder, &insn);

  /* goto after the whole thing */
  post_label = dang_builder_create_label (builder);
  dang_builder_add_jump (builder, post_label);

  for (i = 0; i < N; i++)
    {
      DangExpr *body = cb_exprs[i]->function.args[cb_exprs[i]->function.n_args - 1];

      /* Implement each catch block */
      dang_builder_define_label (builder, clauses[i].target);
      dang_builder_push_local_scope (builder);
      if (clauses[i].var_id != DANG_VAR_ID_INVALID)
        {
          dang_builder_note_var_create (builder, clauses[i].var_id);
          dang_builder_bind_local_var (builder,
                                                cb_exprs[i]->function.args[1]->bareword.name,
                                                clauses[i].var_id);
        }
      dang_compile (body, builder, &void_flags, result);
      if (result->type == DANG_COMPILE_RESULT_ERROR)
        return;
      dang_compile_result_clear (result, builder);

      /* This destroys clauses[i].var_id */
      dang_builder_pop_local_scope (builder);

      /* Goto after the whole thing,
         unless this is the last catch block,
         in which case that's unnecessary. */
      if (i + 1 < N)
        dang_builder_add_jump (builder, post_label);
    }
  dang_builder_define_label (builder, post_label);
  dang_compile_result_init_void (result);
}


DANG_BUILTIN_METAFUNCTION(catch);

#define syntax_check__catch_block "TBA|A"
#define annotate__catch_block NULL
#define compile__catch_block NULL

DANG_BUILTIN_METAFUNCTION(catch_block);
/* === mf-closure.c === */

/* $closure(rettype, params, body) */
#define syntax_check__closure "TAA"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__closure)
{
  /* Do not annotate the body of the closure,
     just create a Stub for it.  (Plus construct a closure if
     any local variables are used.) */
  /* $closure(rettype, arguments, body) */
  DangValueType *type;
  DangSignature *sig;
  char **param_names;
  unsigned i;
  unsigned n_closure_params = 0;
  DangVarId *closure_var_ids;
  type = * (DangValueType **) expr->function.args[0]->value.value;
  sig = dang_signature_parse (expr->function.args[1], type, error);
  if (sig == NULL)
    return FALSE;

  /* Figure out what variables to collect
     from the user's local variables. */
  param_names = dang_newa (char *, sig->n_params);
  for (i = 0; i < sig->n_params; i++)
    param_names[i] = (char*) sig->params[i].name;
  dang_mf_gather_closure_params (sig->n_params, param_names, var_table,
                         expr->function.args[2], &n_closure_params,
                         &closure_var_ids);

  /* Compute the signature of the function + params */
  unsigned full_n_params;
  DangFunctionParam *full_params;
  DangSignature *full_sig;
  DangExprTag *tag;
  DangExpr *body;
  full_n_params = sig->n_params + n_closure_params;
  full_params = dang_new (DangFunctionParam, full_n_params);
  memcpy (full_params, sig->params, sig->n_params * sizeof (DangFunctionParam));
  for (i = 0; i < n_closure_params; i++)
    {
      DangFunctionParam *out = full_params + sig->n_params + i;
      DangVarId id = closure_var_ids[i];
      out->name = dang_var_table_get_name (var_table, id);
      out->dir = DANG_FUNCTION_PARAM_IN;
      out->type = dang_var_table_get_type (var_table, id);
    }
  full_sig = dang_signature_new (sig->return_type, full_n_params, full_params);
  dang_free (full_params);

  /* Add the closure annotations */
  tag = dang_new (DangExprTag, 1);
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_TAG, tag);
  tag->tag_type = DANG_EXPR_TAG_CLOSURE;
  body = dang_expr_ref (expr->function.args[2]);
  /* TODO: untyped functions should probably have access to 
     the some code as the context they are in. */
  tag->info.closure.stub = dang_function_new_stub (imports, full_sig, body,
                                                   NULL, 0, NULL);
                                                   
  tag->info.closure.n_closure_var_ids = n_closure_params;
  tag->info.closure.closure_var_ids = closure_var_ids;
  tag->info.closure.sig = sig;
  tag->info.closure.function_type = dang_value_type_function (sig);

  dang_signature_unref (full_sig);
  dang_expr_unref (body);

  return TRUE;
}


DANG_METAFUNCTION_COMPILE_FUNC_DECLARE (compile__closure)
{
  DangExprTag *tag;
  DangFunction *underlying;
  DangCompileResult func_name_res;

  tag = dang_expr_get_annotation (builder->annotations, expr, DANG_EXPR_ANNOTATION_TAG);
  dang_assert (tag);
  dang_assert (tag->tag_type == DANG_EXPR_TAG_CLOSURE);

  underlying = tag->info.closure.stub;
  dang_compile_result_init_literal (&func_name_res,
                                    dang_value_type_function (underlying->base.sig),
                                    &underlying);

  dang_compile_create_closure (builder,
                               &expr->any.code_position,
                               &func_name_res,
                               tag->info.closure.n_closure_var_ids,
                               tag->info.closure.closure_var_ids,
                               result);
  dang_compile_result_clear (&func_name_res, builder);
  dang_compile_obey_flags (builder, flags, result);
  return;
}

DANG_BUILTIN_METAFUNCTION(closure);
/* === mf-cond.c === */

#define syntax_check__cond "AA*A"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__cond)
{
  DangValueType *rv_type = NULL;
  unsigned i;
  for (i = 0; i < expr->function.n_args; i++)
    {
      if (!dang_expr_annotate_types (annotations, expr->function.args[i], imports, var_table, error))
        return FALSE;
      if (i % 2 == 1 || i + 1 == expr->function.n_args)
        {
          DangExprTag *this_tag;
          this_tag = dang_mf_get_tag (annotations, expr->function.args[i], var_table);
          if (this_tag->tag_type != DANG_EXPR_TAG_VALUE)
            {
              dang_set_error (error, "in ?:, expression had bad tag %s",
                              dang_expr_tag_type_name (this_tag->tag_type));
              return FALSE;
            }
          if (rv_type)
            {
              if (rv_type != this_tag->info.value.type)
                {
                  dang_set_error (error, "in ?:, type-mismatch %s v %s",
                                  rv_type->full_name,
                                  this_tag->info.value.type->full_name);
                  return FALSE;
                }
            }
          else
            {
              rv_type = this_tag->info.value.type;
            }
        }
    }
  dang_assert (rv_type != NULL);
  dang_mf_annotate_value (annotations, expr, rv_type, FALSE, TRUE);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__cond)
{
  DangLabelId end_label, next;
  unsigned i;
  dang_boolean has_next = FALSE;
  DangVarId var_id;
  DangValueType *type = NULL;
  DangCompileResult lvalue_result;
  DangExprTag *tag;
  DANG_UNUSED (flags);

  tag = dang_expr_get_annotation (builder->annotations, expr, DANG_EXPR_ANNOTATION_TAG);
  dang_assert (tag->tag_type == DANG_EXPR_TAG_VALUE);
  type = tag->info.value.type;
  var_id = dang_builder_add_tmp (builder, type);
  end_label = dang_builder_create_label (builder);

  dang_builder_push_local_scope (builder);

  dang_compile_result_init_stack (&lvalue_result, type, var_id,
                                  FALSE, TRUE, FALSE);
  for (i = 0; i < expr->function.n_args; i += 2)
    {
      DangCompileFlags flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
      unsigned body_index;
      DangCompileResult subresult;
      if (has_next)
        {
          dang_builder_define_label (builder, next);
          has_next = FALSE;
        }
      if (i + 1 < expr->function.n_args)
        {
          dang_builder_push_tmp_scope (builder);
          dang_compile (expr->function.args[i], builder, &flags, result);
          if (result->type == DANG_COMPILE_RESULT_ERROR)
            return;
          next = dang_builder_create_label (builder);
          has_next = TRUE;
          dang_builder_add_jump_if_zero (builder, result, next);
          dang_builder_pop_tmp_scope (builder);
          body_index = i + 1;
          dang_compile_result_clear (result, builder);
        }
      else
        body_index = i;


      dang_builder_push_tmp_scope (builder);
      dang_compile (expr->function.args[body_index], builder,
                    &flags, &subresult);
      if (subresult.type == DANG_COMPILE_RESULT_ERROR)
        return;
      dang_assert (dang_value_type_is_autocast (type, subresult.any.return_type));
      dang_builder_add_assign (builder, &lvalue_result, &subresult);
      lvalue_result.stack.was_initialized = TRUE;
      dang_compile_result_clear (&subresult, builder);
      dang_builder_pop_tmp_scope (builder);
      if (i + 2 < expr->function.n_args)
        dang_builder_add_jump (builder, end_label);
    }

  dang_compile_result_clear (&lvalue_result, builder);
  dang_builder_pop_local_scope (builder);
  if (has_next)
    dang_builder_define_label (builder, next);
  dang_builder_define_label (builder, end_label);
  dang_compile_result_init_stack (result, type, var_id,
                                  TRUE, FALSE, TRUE);
}

DANG_BUILTIN_METAFUNCTION(cond);
/* === mf-continue.c === */

#define syntax_check__continue   "|I"
#define annotate__continue dang_mf_annotate__none
#define compile__continue dang_mf_compile__generic_scoped_label_jump

DANG_BUILTIN_METAFUNCTION(continue);
/* === mf-create_struct.c === */
/* TODO: handle literal values in the expression more efficiently. */


#define syntax_check__create_struct "T$create_struct_arg_list()"
#define syntax_check__create_union "TB$create_struct_arg_list()"
#define syntax_check__create_struct_arg_list "*A"
#define syntax_check__named_param "BA"

DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__create_struct)
{
  DangValueType *type;
  DangExpr *argp;
  unsigned i;
  dang_boolean got_named_param = FALSE;
  type = *(DangValueType**) expr->function.args[0]->value.value;
  if (!dang_value_type_is_struct (type))
    {
      dang_set_error (error, "constructing non-structure type %s ("DANG_CP_FORMAT")",
                      type->full_name, DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }
  argp = expr->function.args[1];
  for (i = 0; i < argp->function.n_args; i++)
    {
      DangExpr *a = argp->function.args[i];
      if (dang_expr_is_function (a, "$named_param"))
        {
          got_named_param = TRUE;
          a = a->function.args[1];
        }
      else
        {
          if (got_named_param)
            {
              dang_set_error (error, "all non-named structure parameters must precede the named parameters (constructing %s) ("DANG_CP_FORMAT")",
                              type->full_name, DANG_CP_EXPR_ARGS (argp->function.args[i]));
              return FALSE;
            }
        }
      if (!dang_expr_annotate_types (annotations, a, imports, var_table, error))
        return FALSE;
    }
  dang_mf_annotate_value (annotations, expr,
                          *(DangValueType**) expr->function.args[0]->value.value,
                          FALSE, TRUE);
  return TRUE;
}

static void
do_compile_create_struct (DangBuilder *builder,
                          DangValueType *struct_type,
                          DangValueType *var_type,
                          dang_boolean is_union,            /* must be 0 or 1 */
                          DangExpr *argp,
                          DangCompileFlags *flags,
                          DangCompileResult *result)
{
  DangValueTypeStruct *stype = (DangValueTypeStruct *) struct_type;
  unsigned n_members = stype->n_members;
  unsigned i, j;
  unsigned n_args = argp->function.n_args;
  DangVarId svar_id;
  DangCompileFlags elt_flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  unsigned *arg_to_member_index;
  DangExpr **member_to_expr;
  char *prototype;
  DangInsn insn;

  DANG_UNUSED (flags);

  arg_to_member_index = dang_newa (unsigned, n_args);
  member_to_expr = dang_newa (DangExpr *, n_members);
  for (i = 0; i < n_args; i++)
    arg_to_member_index[i] = (unsigned)-1;
  for (i = 0; i < n_members; i++)
    member_to_expr[i] = NULL;
  for (i = 0; i < n_args; i++)
    {
      DangExpr *a = argp->function.args[i];
      if (dang_expr_is_function (a, "$named_param"))
        {
          /* find argument */
          const char *name = a->function.args[0]->bareword.name;
          for (j = 0; j < stype->n_members; j++)
            {
              if (stype->members[j].name != NULL
               && strcmp (name, stype->members[j].name) == 0)
                break;
            }
          if (j == stype->n_members)
            {
              dang_compile_result_set_error (result, &a->any.code_position,
                                             "named parameter '%s' to struct %s did not exist",
                                             name, struct_type->full_name);
              return;
            }
          arg_to_member_index[i] = j;
          a = a->function.args[1];
        }
      else
        {
          arg_to_member_index[i] = i + is_union;
        }
      if (member_to_expr[arg_to_member_index[i]] != NULL)
        {
          dang_compile_result_set_error (result, &a->any.code_position,
                                         "two initializers given for %s in %s",
                                         stype->members[arg_to_member_index[i]].name,
                                         struct_type->full_name);
          return;
        }
      member_to_expr[arg_to_member_index[i]] = a;
    }

  /* Allocate the prototype structure */
  prototype = dang_malloc0 (struct_type->sizeof_instance);

  for (i = 0; i < n_members; i++)
    {
      DangStructMember *mem = stype->members + i;
      DangValueType *mtype = mem->type;
      if (member_to_expr[i] != NULL)
        {
          DangExpr *m = member_to_expr[i];
          if (m->type == DANG_EXPR_TYPE_VALUE)
            /* Setup literal value */
            dang_value_init_assign (mtype, prototype + mem->offset, m->value.value);
        }
      else
        {
          if (mem->has_default_value)
            /* Copy default value */
            dang_value_init_assign (mtype, prototype + mem->offset, mem->default_value);
        }
    }

  /* Add assign-literal insn */
  dang_insn_init (&insn, DANG_INSN_TYPE_ASSIGN);
  insn.assign.source.location = DANG_INSN_LOCATION_LITERAL;
  insn.assign.source.type = struct_type;
  insn.assign.source.value = prototype;
  insn.assign.target.location = DANG_INSN_LOCATION_STACK;
  insn.assign.target.type = struct_type;
  svar_id = dang_builder_add_tmp (builder, struct_type);
  insn.assign.target.var = svar_id;
  insn.assign.target_uninitialized = TRUE;
  dang_builder_note_var_create (builder, svar_id);
  dang_builder_add_insn (builder, &insn);

  for (i = 0; i < n_members; i++)
    {
      /* Compile non-literal pieces */
      DangStructMember *mem = stype->members + i;
      DangValueType *mtype = mem->type;
      if (member_to_expr[i] != NULL
       && member_to_expr[i]->type != DANG_EXPR_TYPE_VALUE)
        {
          DangExpr *m = member_to_expr[i];
          DangCompileResult subres, lvalue;
          DangVarId mem_var_id;
          dang_builder_push_tmp_scope (builder);
          dang_compile (m, builder, &elt_flags, &subres);
          mem_var_id = dang_builder_add_local_alias (builder, svar_id, mem->offset, mtype);
          dang_builder_note_var_create (builder, mem_var_id);
          dang_compile_result_init_stack (&lvalue, mtype, mem_var_id, TRUE, TRUE, FALSE);
          dang_builder_add_assign (builder, &lvalue, &subres);
          dang_compile_result_clear (&subres, builder);
          dang_compile_result_clear (&lvalue, builder);
          dang_builder_pop_tmp_scope (builder);
        }
    }

  dang_compile_result_init_stack (result, var_type, svar_id,
                                  TRUE, FALSE, TRUE);
}

DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__create_struct)
{
  DangValueType *type = *(DangValueType**) expr->function.args[0]->value.value;
  do_compile_create_struct (builder, type, type, FALSE,
                            expr->function.args[1], flags, result);
}


DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__create_union)
{
  DangValueType *type;
  DangExpr *argp;
  unsigned i, case_index;
  dang_boolean got_named_param = FALSE;
  const char *tag;
  DangValueTypeUnion *utype;
  DangValueType *stype;
  type = *(DangValueType**) expr->function.args[0]->value.value;
  if (!dang_value_type_is_union (type))
    {
      dang_set_error (error, "constructing non-union type %s ("DANG_CP_FORMAT")",
                      type->full_name, DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }
  utype = (DangValueTypeUnion *) type;
  tag = expr->function.args[1]->bareword.name;

  /* lookup structure */
  for (i = 0; i < utype->n_cases; i++)
    if (strcmp (utype->cases[i].name, tag) == 0)
      break;
  if (i == utype->n_cases)
    {
      dang_set_error (error, "constructing union '%s': no case '%s' ("DANG_CP_FORMAT")",
                      type->full_name, tag, DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }
  stype = utype->cases[i].struct_type;
  case_index = i;

  argp = expr->function.args[2];
  for (i = 0; i < argp->function.n_args; i++)
    {
      DangExpr *a = argp->function.args[i];
      if (dang_expr_is_function (a, "$named_param"))
        {
          got_named_param = TRUE;
          a = a->function.args[1];
        }
      else
        {
          if (got_named_param)
            {
              dang_set_error (error, "all non-named structure parameters must precede the named parameters (constructing %s) ("DANG_CP_FORMAT")",
                              type->full_name, DANG_CP_EXPR_ARGS (argp->function.args[i]));
              return FALSE;
            }
        }
      if (!dang_expr_annotate_types (annotations, a, imports, var_table, error))
        return FALSE;
    }
  dang_mf_annotate_value (annotations, expr,
                          *(DangValueType**) expr->function.args[0]->value.value,
                          FALSE, TRUE);
  return TRUE;
}

DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__create_union)
{
  DangValueType *type = *(DangValueType**) expr->function.args[0]->value.value;
  DangValueTypeUnion *utype = (DangValueTypeUnion*) type;
  const char *tag = expr->function.args[1]->bareword.name;
  DangExpr *argp = expr->function.args[2];
  unsigned i;

  /* XXX: cache this computation from annotate */
  for (i = 0; i < utype->n_cases; i++)
    if (strcmp (utype->cases[i].name, tag) == 0)
      break;
  dang_assert (i < utype->n_cases);
  do_compile_create_struct (builder, utype->cases[i].struct_type, type, TRUE,
                            argp, flags, result);
}



DANG_BUILTIN_METAFUNCTION (create_struct);
DANG_BUILTIN_METAFUNCTION__SYNTAX_CHECK_ONLY (create_struct_arg_list);
DANG_BUILTIN_METAFUNCTION__SYNTAX_CHECK_ONLY (named_param);
DANG_BUILTIN_METAFUNCTION (create_union);
/* === mf-define_enum.c === */

#define syntax_check__define_enum  "B$enum_values()"
#define syntax_check__enum_values  "*$enum_value()"
#define syntax_check__enum_value   "B|BI"

#define annotate__define_enum dang_mf_annotate__none

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__define_enum)
{
  const char *name = expr->function.args[0]->bareword.name;
  unsigned n_values = expr->function.args[1]->function.n_args;
  unsigned i;
  DangEnumValue *values = dang_new (DangEnumValue, n_values);
  DangValueType *rv;
  DangError *error = NULL;
  DangNamespace *ns;
  DANG_UNUSED (flags);
  for (i = 0; i < n_values; i++)
    {
      DangExpr *ev_expr = expr->function.args[1]->function.args[i];
      values[i].name = ev_expr->function.args[0]->bareword.name;
      if (ev_expr->function.n_args == 1)
        values[i].code = (i == 0) ? 0 : (values[i-1].code + 1);
      else
        values[i].code = * (uint32_t *) ev_expr->function.args[0]->value.value;
    }
  rv = dang_value_type_new_enum (name, 0, n_values, values, &error);
  dang_free (values);
  if (rv == NULL)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "error defining enum: %s",
                                     error->message);
      dang_error_unref (error);
      return;
    }
  ns = builder->imports->default_definition_namespace;
  if (ns == NULL)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "no definition namespace for %s",
                                     name);
      return;
    }
  if (!dang_namespace_add_type (ns, name, rv, &error))
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "error adding enum to namespace: %s",
                                     error->message);
      dang_error_unref (error);
      return;
    }
  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(define_enum);
DANG_BUILTIN_METAFUNCTION__SYNTAX_CHECK_ONLY(enum_values);
DANG_BUILTIN_METAFUNCTION__SYNTAX_CHECK_ONLY(enum_value);
/* === mf-define_function.c === */
/* $define_function(BAREWORD, RETVAL_TYPE, ARG_LIST, BODY) */

#define syntax_check__define_function   "BT$arguments()A"

#define annotate__define_function    dang_mf_annotate__none

DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__define_function)
{
  DangExprBareword *bw_expr;
  const char *symbol_name;
  DangNamespace *ns;
  DangNamespaceSymbol *symbol;
  DangValueType *ret_type;
  DangError *error = NULL;
  DangExpr *args_sig_expr;
  DangSignature *sig;
  DangFunction *function;

  DangExpr *body_expr;
  DANG_UNUSED (flags);

  /* locate namespace in BAREWORD */
  bw_expr = &expr->function.args[0]->bareword;
  symbol_name = bw_expr->name;
  ns = builder->imports->default_definition_namespace;
  if (ns == NULL)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "couldn't find namespace to add definition of %s to",
                                     bw_expr->name);
      return;
    }

  /* make sure it isn't used by a non-function type */
  symbol = dang_namespace_lookup (ns, symbol_name);
  if (symbol != NULL && symbol->type != DANG_NAMESPACE_SYMBOL_FUNCTIONS)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "symbol %s already defined in namespace %s as a %s",
                                     symbol_name, ns->full_name,
                                     dang_namespace_symbol_type_name (symbol->type));
      return;
    }

  /* parse arguments and return-value info */
  ret_type = * (DangValueType**) expr->function.args[1]->value.value;
  if (ret_type == dang_value_type_void ())
    ret_type = NULL;
  args_sig_expr = expr->function.args[2];
  sig = dang_signature_parse (args_sig_expr, ret_type, &error);
  if (sig == NULL)
    {
      dang_compile_result_set_error (result, &args_sig_expr->any.code_position,
                                     "error parsing signature: %s",
                                     error->message);
      dang_error_unref (error);
      return;
    }
  if (!dang_namespace_check_function (ns, symbol_name, sig, &error))
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "function %s in namespace %s: %s",
                                     symbol_name, ns->full_name,
                                     error->message);
      dang_error_unref (error);
      dang_signature_unref (sig);
      return;
    }
  body_expr = expr->function.args[3];
  if (sig->is_templated)
    {
      DangFunctionFamily *ff;
      const char *bad_name;
      DangExpr *bad_expr;
      DangUtilArray tparams = DANG_UTIL_ARRAY_STATIC_INIT (DangValueType *);
      unsigned i;

      /* Collect the template parameters. */
      if (sig->return_type)
        dang_type_gather_template_params (sig->return_type, &tparams);
      for (i = 0; i < sig->n_params; i++)
        dang_type_gather_template_params (sig->params[i].type, &tparams);

      if (dang_expr_contains_disallowed_template_param (body_expr, tparams.len, tparams.data, &bad_name, &bad_expr))
        {
          dang_compile_result_set_error (result, &args_sig_expr->any.code_position,
                                         "disallowed template-parameter %%%%%s in %s ("DANG_CP_FORMAT")",
                                         bad_name, symbol_name,
                                         DANG_CP_EXPR_ARGS (bad_expr));
          dang_util_array_clear (&tparams);
          dang_signature_unref (sig);
          return;
        }
      dang_util_array_clear (&tparams);


      ff = dang_function_family_new_template (symbol_name, builder->imports, sig, body_expr, &error);
      dang_signature_unref (sig);
                                         
      if (ff == NULL)
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "making templated function: %s",
                                         error->message);
          dang_error_unref (error);
          dang_function_family_unref (ff);
          return;
        }
      if (!dang_namespace_add_function_family (ns, symbol_name, ff, &error))
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "error adding %s to %s: %s",
                                         symbol_name, ns->full_name, error->message);
          dang_function_family_unref (ff);
          return;
        }
      dang_function_family_unref (ff);
    }
  else
    {
      const char *bad_name;
      DangExpr *bad_expr;
      if (dang_expr_contains_disallowed_template_param (body_expr, 0, NULL,
                                                        &bad_name, &bad_expr))
        {
          dang_compile_result_set_error (result, &args_sig_expr->any.code_position,
                                         "function body for %s contains a template parameter %%%%%s, but signature does not (at "DANG_CP_FORMAT")",
                                         symbol_name, bad_name,
                                         DANG_CP_EXPR_ARGS (bad_expr));
          dang_signature_unref (sig);
          return;
        }

      /* TODO: friend-declarations support of some type */
      function = dang_function_new_stub (builder->imports, sig,
                                         body_expr,
                                         NULL, 0, NULL);
      dang_signature_unref (sig);

      /* add it to the namespace */
      if (!dang_namespace_add_function (ns, symbol_name, function, &error))
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "error adding %s to %s: %s",
                                         symbol_name, ns->full_name, error->message);
          dang_function_unref (function);
          dang_error_unref (error);
          return;
        }
      dang_function_unref (function);
    }


  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(define_function);

#define syntax_check__arguments  "*$argument()"
#define compile__arguments NULL
#define annotate__arguments NULL
DANG_BUILTIN_METAFUNCTION(arguments);

#define syntax_check__argument  "BTB"
#define compile__argument NULL
#define annotate__argument NULL
DANG_BUILTIN_METAFUNCTION(argument);
/* === mf-define_global.c === */

#define syntax_check__define_global  "TB"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__define_global)
{
  DangValueType *type = * (DangValueType **) expr->function.args[0]->value.value;
  const char *name = expr->function.args[1]->bareword.name;
  DangNamespace *def_ns = imports->default_definition_namespace;
  DangNamespaceSymbol *symbol;
  DANG_UNUSED (var_table);

  dang_assert (def_ns != NULL);

  if (type->internals.is_templated)
    {
      dang_set_error (error, "cannot create global of non-instantiable type %s (named '%s' in namespace '%s' ("DANG_CP_FORMAT")",
                      type->full_name,
                      name, def_ns->full_name,
                      DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }


  /* see if a global by that name already exists (in the definition ns) */
  symbol = dang_namespace_lookup (def_ns, name);
  if (symbol != NULL)
    {
      dang_set_error (error, "cannot create global named '%s' in namespace '%s', %s already exists",
                      name, def_ns->full_name, dang_namespace_symbol_type_name (symbol->type));
      return FALSE;
    }
  dang_mf_annotate_value (annotations, expr, type, TRUE, FALSE);
  return TRUE;
}
static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__define_global)
{
  /* XXX: technically, we should get the NS from an annotation. */

  DangNamespace *ns;
  DangExprTag *tag;
  unsigned ns_offset;
  DangError *error = NULL;
  dang_assert (expr->function.n_args == 2);
  ns = builder->imports->default_definition_namespace;
  tag = dang_expr_get_annotation (builder->annotations, expr, DANG_EXPR_ANNOTATION_TAG);
  dang_assert (tag && tag->tag_type == DANG_EXPR_TAG_VALUE);
  if (!dang_namespace_add_global (ns,
                                  expr->function.args[1]->bareword.name,
                                  tag->info.value.type,
                                  NULL, &ns_offset, &error))
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "error adding variable %s: %s",
                                     expr->function.args[1]->bareword.name,
                                     error->message);
      return;
    }

  dang_compile_result_init_global (result, tag->info.value.type,
                                   ns, ns_offset,
                                   TRUE, TRUE);
  dang_compile_obey_flags (builder, flags, result);
}

DANG_BUILTIN_METAFUNCTION(define_global);
/* === mf-define_global_infer_type.c === */

#define syntax_check__define_global_infer_type  "BA"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__define_global_infer_type)
{
  const char *name = expr->function.args[0]->bareword.name;
  DangNamespace *def_ns = imports->default_definition_namespace;
  DangNamespaceSymbol *symbol;
  DangExprTag *rhs_tag;
  dang_assert (def_ns != NULL);

  /* see if a global by that name already exists (in the definition ns) */
  symbol = dang_namespace_lookup (def_ns, name);
  if (symbol != NULL)
    {
      dang_set_error (error, "cannot create global named '%s' in namespace '%s', %s already exists",
                      name, def_ns->full_name, dang_namespace_symbol_type_name (symbol->type));
      return FALSE;
    }
  /* infer the type from the RHS. */
  if (!dang_expr_annotate_types (annotations, expr->function.args[1], imports, var_table, error))
    return FALSE;
  rhs_tag = dang_mf_get_tag (annotations, expr->function.args[1], var_table);
  dang_assert (rhs_tag->tag_type == DANG_EXPR_TAG_VALUE);
  //dang_mf_annotate_value (annotations, expr, rhs_tag->info.value.type, FALSE, TRUE);
  dang_mf_annotate_statement (annotations, expr);
  dang_mf_annotate_value (annotations, expr->function.args[0], rhs_tag->info.value.type, TRUE, FALSE);
  return TRUE;
}
static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__define_global_infer_type)
{
  /* XXX: technically, we should get the NS from an annotation. */


  DangNamespace *ns;
  DangExprTag *tag;
  unsigned ns_offset;
  DangError *error = NULL;
  DangCompileFlags our_flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  DangCompileResult subresult;
  DangCompileResult lvalue;
  dang_assert (flags->permit_void);
  ns = builder->imports->default_definition_namespace;
  tag = dang_expr_get_annotation (builder->annotations, expr->function.args[0], DANG_EXPR_ANNOTATION_TAG);
  dang_assert (tag && tag->tag_type == DANG_EXPR_TAG_VALUE);
  if (!dang_namespace_add_global (ns,
                                  expr->function.args[0]->bareword.name,
                                  tag->info.value.type,
                                  NULL, &ns_offset, &error))
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "error adding variable %s: %s",
                                     expr->function.args[0]->bareword.name,
                                     error->message);
      return;
    }

  dang_compile (expr->function.args[1], builder, &our_flags, &subresult);
  if (subresult.type == DANG_COMPILE_RESULT_ERROR)
    {
      *result = subresult;
      return;
    }
  if (subresult.any.return_type != tag->info.value.type)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "type-mismatching compiling assign to global: %s v %s",
                                     subresult.any.return_type->full_name,
                                     tag->info.value.type->full_name);
      dang_compile_result_clear (&subresult, builder);
      return;
    }
  dang_compile_result_init_global (&lvalue, subresult.any.return_type,
                                   ns, ns_offset,
                                   TRUE, FALSE);
  
  dang_builder_add_assign (builder, &lvalue, &subresult);
  dang_compile_result_init_void (result);
  dang_compile_result_clear (&subresult, builder);
  dang_compile_result_clear (&lvalue, builder);
}

DANG_BUILTIN_METAFUNCTION(define_global_infer_type);
/* === mf-define_object.c === */


/* $define_object(NAME, TYPE, BODY)  */
#define syntax_check__define_object  "BT$object_elements()"

/* $object_define(NAME, TYPE, $object_elements(...))
   $object_elements([$object_method,$object_member]*)
   $object_method($method_flags(...), NAME, RET_TYPE, $arguments(...), statement_or_void)
   $object_member($member_flags(...), TYPE, NAME)
   $object_constructor(opt_name, $arguments(...), BODY)
   */
#define syntax_check__object_elements   "@"   /* sigh, no way to do this */
#define syntax_check__object_method     "$method_flags()BT$arguments()A"
#define syntax_check__object_member     "$member_flags()TB"
#define syntax_check__object_constructor   "B$arguments()A"
#define syntax_check__method_flags      "*B"
#define syntax_check__member_flags      "*B"

static dang_boolean
parse_object_member_flags (DangExpr *expr,
                           DangMemberFlags *flags_out,
                           DangError **error)
{
  unsigned i;
  DangMemberFlags flags = DANG_MEMBER_PRIVATE_READABLE|DANG_MEMBER_PRIVATE_WRITABLE;
  for (i = 0; i < expr->function.n_args; i++)
    {
      const char *str;
      dang_assert (expr->function.args[i]->type == DANG_EXPR_TYPE_BAREWORD);
      str = expr->function.args[i]->bareword.name;
      switch (str[0])
        {
        case 'p':
          if (strcmp (str, "public") == 0)
            {
              flags |= DANG_MEMBER_PUBLIC_WRITABLE|DANG_MEMBER_PUBLIC_READABLE
                     | DANG_MEMBER_PROTECTED_WRITABLE|DANG_MEMBER_PROTECTED_READABLE;
              continue;
            }
          else if (strcmp (str, "protected") == 0)
            {
              flags |= DANG_MEMBER_PROTECTED_WRITABLE|DANG_MEMBER_PROTECTED_READABLE;
              continue;
            }
          else if (strcmp (str, "private") == 0)
            {
              continue;
            }
          break;
        case 'r':
          if (strcmp (str, "readonly") == 0)
            {
              flags |= DANG_MEMBER_PROTECTED_READABLE|DANG_MEMBER_PUBLIC_READABLE;
              continue;
            }
          break;
        default:
          break;
        }
      dang_set_error (error, "unexpected member flag %s", str);
      return FALSE;
    }
  *flags_out = flags;
  return TRUE;
}
static dang_boolean
parse_object_method_flags (DangExpr *expr,
                           DangMethodFlags *flags_out,
                           DangError **error)
{
  unsigned i;
  DangMethodFlags flags = 0;
  for (i = 0; i < expr->function.n_args; i++)
    {
      const char *str;
      dang_assert (expr->function.args[i]->type == DANG_EXPR_TYPE_BAREWORD);
      str = expr->function.args[i]->bareword.name;
      switch (str[0])
        {
        case 'a':
          if (strcmp (str, "abstract") == 0)
            {
              flags |= DANG_METHOD_ABSTRACT;
              continue;
            }
          break;
        case 'f':
          if (strcmp (str, "final") == 0)
            {
              flags |= DANG_METHOD_FINAL;
              continue;
            }
          break;
        case 'm':
          if (strcmp (str, "mutable") == 0)
            {
              flags |= DANG_METHOD_MUTABLE;
              continue;
            }
          break;
        case 'p':
          if (strcmp (str, "public") == 0)
            {
              flags |= DANG_METHOD_PUBLIC;
              continue;
            }
          else if (strcmp (str, "protected") == 0)
            {
              flags |= DANG_METHOD_PROTECTED;
              continue;
            }
          else if (strcmp (str, "private") == 0)
            {
              flags |= DANG_METHOD_PRIVATE;
              continue;
            }
          break;
        case 'r':
          if (strcmp (str, "readonly") == 0)
            {
              flags |= DANG_METHOD_PUBLIC_READONLY;
              continue;
            }
          break;

        case 's':
          if (strcmp (str, "static") == 0)
            {
              flags |= DANG_METHOD_STATIC;
              continue;
            }
          break;
        }
      dang_set_error (error, "unexpected method flag %s", str);
      return FALSE;
    }
  *flags_out = flags;
  return TRUE;
}
static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__define_object)
{
  DangValueType *parent_type;
  DangValueType *object_type;
  unsigned n_elts;
  DangExpr **elts;
  unsigned i;
  const char *name;
  DangNamespace *ns;

  DANG_UNUSED (annotations);
  DANG_UNUSED (var_table);

  dang_assert (expr->function.n_args == 3);
  dang_assert (expr->function.args[0]->type == DANG_EXPR_TYPE_BAREWORD);
  dang_assert (expr->function.args[1]->type == DANG_EXPR_TYPE_VALUE);

  name = expr->function.args[0]->bareword.name;
  parent_type = * (DangValueType **) expr->function.args[1]->value.value;
  if (parent_type == NULL || parent_type == dang_value_type_void ())
    parent_type = dang_value_type_object ();
  object_type = dang_object_type_subclass (parent_type, name);

  dang_assert (expr->function.args[2]->type == DANG_EXPR_TYPE_FUNCTION);
  n_elts = expr->function.args[2]->function.n_args;
  elts = expr->function.args[2]->function.args;
  for (i = 0; i < n_elts; i++)
    {
      DangExpr *elt = elts[i];
      dang_assert (elt->type == DANG_EXPR_TYPE_FUNCTION);
      if (strcmp (elt->function.name, "$object_member") == 0)
        {
          /* parse flags */
          DangMemberFlags flags;
          DangValueType *type;
          const void *default_value;
          if (!parse_object_member_flags (elt->function.args[0], &flags, error))
            return FALSE;
            
          /* parse type */
          if (elt->function.args[1]->type != DANG_EXPR_TYPE_VALUE
           || elt->function.args[1]->value.type != dang_value_type_type ())
            {
              dang_set_error (error, "expected arg 2 of $object_member to be a type");
              return FALSE;
            }
          type = * (DangValueType **) elt->function.args[1]->value.value;
          if (type == NULL)
            dang_die ("member with NULL type");

          if (elt->function.n_args == 4)
            {
              if (elt->function.args[3]->type != DANG_EXPR_TYPE_VALUE)
                {
                  dang_set_error (error, "arg 4 of $object_member was not a literal value");
                  return FALSE;
                }
              if (elt->function.args[3]->value.type != type)
                {
                  dang_set_error (error, "arg 4 (default-value) of $object_member was a literal value of type %s, but we need a value of type %s ("DANG_CP_FORMAT")",
                                  elt->function.args[3]->value.type->full_name,
                                  type->full_name,
                                  DANG_CP_ARGS (elt->function.args[3]->any.code_position));
                  return FALSE;
                }
              default_value = elt->function.args[3]->value.value;
            }
          else
            default_value = NULL;

          if (!dang_object_add_member (object_type,
                                       elt->function.args[2]->bareword.name,
                                       flags, type, default_value, error))
            return FALSE;
        }
      else if (strcmp (elts[i]->function.name, "$object_method") == 0)
        {
          DangValueType *ret_type;
          /* parse flags */
          DangMethodFlags flags;
          DangSignature *sig, *new_sig;
          DangExpr *body;
          const char *name;
          dang_assert (elt->function.n_args == 5);
          dang_assert (elt->function.args[1]->type == DANG_EXPR_TYPE_BAREWORD);
          name = elt->function.args[1]->bareword.name;


          if (!parse_object_method_flags (elt->function.args[0], &flags, error))
            return FALSE;


          /* parse signature (as though it were not a method) */
          dang_assert (elt->function.args[2]->type == DANG_EXPR_TYPE_VALUE);
          dang_assert (elt->function.args[2]->value.type == dang_value_type_type ());
          ret_type = * (DangValueType **) elt->function.args[2]->value.value;
          if (ret_type == dang_value_type_void ())
            ret_type = NULL;
          sig = dang_signature_parse (elt->function.args[3], ret_type, error);
          if (sig == NULL)
            return FALSE;

          /* unless it's a static method, create a new sig including 'this' */
          if ((flags & DANG_METHOD_STATIC) == 0)
            {
              DangFunctionParam *new_params = dang_new (DangFunctionParam, sig->n_params + 1);
              memcpy (new_params + 1, sig->params, sig->n_params * sizeof (DangFunctionParam));
              new_params[0].dir = DANG_FUNCTION_PARAM_IN;
              new_params[0].name = "this";
              new_params[0].type = object_type;
              new_sig = dang_signature_new (sig->return_type, sig->n_params + 1, new_params);
              dang_free (new_params);
            }
          else
            new_sig = dang_signature_ref (sig);

          /* do we have a body or a declaration? */
          if (elt->function.args[4]->type == DANG_EXPR_TYPE_BAREWORD
            && strcmp (elt->function.args[4]->bareword.name, "$void") == 0)
            body = NULL;
          else
            body = elt->function.args[4];

          /* is having / not having a body compatible with flags? */
          if (body == NULL)
            {
              if ((flags & DANG_METHOD_ABSTRACT) == 0)
                {
                  dang_set_error (error, "no method given, but function not abstract");
                  dang_signature_unref (new_sig);
                  return FALSE;
                }
            }
          else
            {
              if ((flags & DANG_METHOD_ABSTRACT) != 0)
                {
                  dang_set_error (error, "method given, even though function was marked abstract");
                  return FALSE;
                }
            }
          
          if (body != NULL)
            {
              DangFunction *func;
              func = dang_function_new_stub (imports, new_sig, body,
                                             object_type, 0, NULL);
              if (!dang_object_add_method (object_type,
                                           name, flags,
                                           func, error))
                return FALSE;
              dang_function_unref (func);
            }
          else
            {
              if (!dang_object_add_abstract_method (object_type,
                                                    name, flags,
                                                    new_sig, error))
                return FALSE;
            }
          dang_signature_unref (new_sig);
          dang_signature_unref (sig);
        }
      else if (strcmp (elts[i]->function.name, "$object_constructor") == 0)
        {
          const char *name = elts[i]->function.args[0]->bareword.name;
          DangSignature *sig = dang_signature_parse (elt->function.args[1], NULL, error);
          DangSignature *real_sig;
          DangFunction *stub;
          DangFunctionParam *real_params;
          dang_boolean rv;
          if (sig == NULL)
            return FALSE;
          if (strcmp (name, "$void") == 0)
            name = NULL;
          real_params = dang_new (DangFunctionParam, sig->n_params + 1);
          memcpy (real_params + 1, sig->params, sig->n_params * sizeof(DangFunctionParam));
          real_params[0].dir = DANG_FUNCTION_PARAM_IN;
          real_params[0].type = object_type;
          real_params[0].name = "this";
          real_sig = dang_signature_new (NULL, sig->n_params + 1, real_params);
          stub = dang_function_new_stub (imports, real_sig,
                                         elt->function.args[2],
                                         object_type, 0, NULL);
          rv = dang_object_add_constructor (object_type, name, stub, error);
          dang_free (real_params);
          dang_function_unref (stub);
          dang_signature_unref (sig);
          dang_signature_unref (real_sig);
          if (!rv)
            return FALSE;
        }
      else
        {
          dang_die ("expected $object_member or $object_method, got %s",
                    elts[i]->function.name);
        }
    }

  {
    DangExprTag *tag = dang_new (DangExprTag, 1);
    dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_TAG, tag);
    tag->tag_type = DANG_EXPR_TAG_OBJECT_DEFINE;
    tag->info.object_define.type = object_type;
  }

  ns = imports->default_definition_namespace;
  if (!dang_namespace_add_type (ns, name, object_type, error))
    return FALSE;
  return TRUE;
}


static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__define_object)
{
  DangExprTag *tag;
  DANG_UNUSED (flags);
  tag = dang_expr_get_annotation (builder->annotations, expr, DANG_EXPR_ANNOTATION_TAG);
  dang_assert (tag != NULL);
  dang_assert (tag->tag_type == DANG_EXPR_TAG_OBJECT_DEFINE);
  dang_object_note_method_stubs (tag->info.object_define.type, builder->function->stub.cc);
  dang_compile_result_init_void (result);
  return;
}

DANG_BUILTIN_METAFUNCTION(define_object);

#define annotate__object_elements    NULL
#define compile__object_elements     NULL
#define annotate__object_method      NULL
#define compile__object_method       NULL
#define annotate__object_member      NULL
#define compile__object_member       NULL
#define annotate__object_constructor NULL
#define compile__object_constructor  NULL
#define annotate__method_flags       NULL
#define compile__method_flags        NULL
#define annotate__member_flags       NULL
#define compile__member_flags        NULL

DANG_BUILTIN_METAFUNCTION(object_elements);
DANG_BUILTIN_METAFUNCTION(object_method);
DANG_BUILTIN_METAFUNCTION(object_member);
DANG_BUILTIN_METAFUNCTION(object_constructor);
DANG_BUILTIN_METAFUNCTION(method_flags);
DANG_BUILTIN_METAFUNCTION(member_flags);
/* === mf-define_struct.c === */

#define syntax_check__define_struct  "B$members()"
#define syntax_check__members        "*$member()"
#define syntax_check__member         "TB|TBV"

#define annotate__define_struct  dang_mf_annotate__none

static dang_boolean
parse_member (DangExpr *expr,
              DangStructMember *out,
              DangCompileResult *result)
{
  DangValueType *type;
  type = * (DangValueType**) expr->function.args[0]->value.value;
  if (expr->function.n_args == 2)
    {
      out->has_default_value = FALSE;
    }
  else
    {
      if (expr->function.args[2]->value.type != type)
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "type mismatch in default value for %s: member has type %s, default value has type %s",
                                         expr->function.args[1]->bareword.name,
                                         type->full_name, expr->function.args[2]->value.type->full_name);
          return FALSE;
        }
      out->has_default_value = TRUE;
      out->default_value = dang_value_copy (expr->function.args[2]->value.type,
                                            expr->function.args[2]->value.value);
    }

  out->type = type;
  out->name = dang_strdup (expr->function.args[1]->bareword.name);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__define_struct)
{
  unsigned i;
  DangExpr *members_expr;
  DangStructMember *members;
  DangValueType *stype;
  DangNamespace *ns;
  DangError *error = NULL;

  members_expr = expr->function.args[1];

  DANG_UNUSED (flags);

  /* parse members */
  members = dang_new (DangStructMember, members_expr->function.n_args);
  for (i = 0; i < members_expr->function.n_args; i++)
    {
      unsigned j;
      if (!parse_member (members_expr->function.args[i], members + i, result))
        {
          dang_free (members);
          return;
        }
      /* TODO: O(N log N) impl */
      for (j = 0; j < i; j++)
        if (strcmp (members[j].name, members[i].name) == 0)
          {
            dang_compile_result_set_error (result, &members_expr->function.args[i]->any.code_position,
                                           "member %s already defined", members[i].name);
            dang_free (members);
            return;
          }
    }

  stype = dang_value_type_new_struct (dang_strdup (expr->function.args[0]->bareword.name), members_expr->function.n_args, members);

  ns = builder->imports->default_definition_namespace;
  if (ns == NULL)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "definitions not allowed here");
      return;
    }
  if (!dang_namespace_add_type (ns, expr->function.args[0]->bareword.name, stype, &error))
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "error adding type %s to namespace %s: %s",
                                     expr->function.args[0]->bareword.name,
                                     ns->full_name,
                                     error->message);
      dang_error_unref (error);
      return;
    }
  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(define_struct);

#define annotate__members NULL
#define compile__members NULL
DANG_BUILTIN_METAFUNCTION(members);

#define annotate__member NULL
#define compile__member NULL
DANG_BUILTIN_METAFUNCTION(member);
/* === mf-define_union.c === */
#include <stdlib.h>

#define syntax_check__define_union             "B$union_cases()"
#define syntax_check__union_cases              "*$union_case()"
#define syntax_check__union_case               "B$members()"

static dang_boolean
parse_union_case (DangExpr      *case_expr,
                  DangUnionCase *out,
                  DangError    **error)
{
  const char *case_name = case_expr->function.args[0]->bareword.name;
  unsigned n_members = case_expr->function.args[1]->function.n_args;
  DangStructMember *members = dang_new (DangStructMember, n_members);
  unsigned i;
  for (i = 0; i < n_members; i++)
    {
      DangExpr *mem_expr = case_expr->function.args[1]->function.args[i];
      members[i].type = *(DangValueType**)(mem_expr->function.args[0]->value.value);
      members[i].name = dang_strdup (mem_expr->function.args[1]->bareword.name);

      /* members[i].offset is initialized by dang_value_type_new_union() */

      members[i].has_default_value = (mem_expr->function.n_args == 3);
      if (members[i].has_default_value)
        {
          DangValueType *type2 = mem_expr->function.args[2]->value.type;
          if (!dang_value_type_is_autocast (members[i].type, type2))
            {
              dang_set_error (error, "type mismatch for default value of %s.%s (%s v %s) ("DANG_CP_FORMAT")",
                              case_name, members[i].name,
                              members[i].type->full_name, type2->full_name,
                              DANG_CP_EXPR_ARGS (mem_expr));
              return FALSE;
            }

          members[i].default_value = dang_value_copy (type2,
                                                      mem_expr->function.args[2]->value.value);
        }
      else
        members[i].default_value = NULL;
    }
  out->name = dang_strdup (case_name);
  out->n_members = n_members;
  out->members = members;
  return TRUE;
}

typedef struct _NameIndex NameIndex;
struct _NameIndex
{
  const char *name;
  unsigned index;
};

static int compare_name_index_by_name (const void *a, const void *b)
{
  const NameIndex *na = a;
  const NameIndex *nb = b;
  return strcmp (na->name, nb->name);
}

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__define_union)
{
  const char *name = expr->function.args[0]->bareword.name;
  unsigned n_cases = expr->function.args[1]->function.n_args;
  DangUnionCase *cases;
  unsigned i;
  NameIndex *ni;
  DangValueType *type;
  DANG_UNUSED (var_table);
  DANG_UNUSED (annotations);
  if (n_cases == 0)
    {
      dang_set_error (error, "empty union definition not allowed ("DANG_CP_FORMAT")",
                      DANG_CP_EXPR_ARGS (expr));
      return FALSE;
    }
  cases = dang_new (DangUnionCase, n_cases);
  ni = dang_new (NameIndex, n_cases);
  for (i = 0; i < n_cases; i++)
    {
      if (!parse_union_case (expr->function.args[1]->function.args[i], cases + i, error))
        return FALSE;
      ni[i].name = cases[i].name;
      ni[i].index = i;
    }

  /* check no cases have duplicate names */
  qsort (ni, n_cases, sizeof (NameIndex), compare_name_index_by_name);
  for (i = 1; i < n_cases; i++)
    if (strcmp (ni[i-1].name, ni[i].name) == 0)
      {
        dang_set_error (error, "two cases named %s in union %s ("DANG_CP_FORMAT" and "DANG_CP_FORMAT")",
                        ni[i].name, name,
                        DANG_CP_EXPR_ARGS (expr->function.args[1]->function.args[ni[i-1].index]),
                        DANG_CP_EXPR_ARGS (expr->function.args[1]->function.args[ni[i].index]));
        return FALSE;
      }
  dang_free (ni);

  /* Create the type and add it to the namespace */
  type = dang_value_type_new_union (name, n_cases, cases);
  if (!dang_namespace_add_type (imports->default_definition_namespace,
                                name, type,
                                error))
    {
      dang_error_add_pos_suffix (*error, &expr->any.code_position);
      return FALSE;
    }

  /* maybe add annotation? */

  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__define_union)
{
  DANG_UNUSED (flags);
  DANG_UNUSED (expr);
  DANG_UNUSED (builder);
  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(define_union);
DANG_BUILTIN_METAFUNCTION__SYNTAX_CHECK_ONLY(union_case);
DANG_BUILTIN_METAFUNCTION__SYNTAX_CHECK_ONLY(union_cases);
/* === mf-do_while.c === */

#define syntax_check__do_while   "AA"
#define annotate__do_while dang_mf_annotate__push_local_scope

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__do_while)
{
  DangLabelId redo_label, continue_label, break_label;

  DangCompileFlags f = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  DangCompileResult res;
  f.permit_void = 0;
  dang_assert (expr->function.n_args == 2);

  DANG_UNUSED (flags);

  redo_label = dang_builder_start_scoped_label (builder, "$redo");
  continue_label = dang_builder_start_scoped_label (builder, "$continue");
  break_label = dang_builder_start_scoped_label (builder, "$break");
  dang_builder_push_local_scope (builder);
  dang_builder_define_label (builder, redo_label);
  dang_compile (expr->function.args[0], builder, &dang_compile_flags_void, &res);
  dang_builder_define_label (builder, continue_label);
  dang_compile_result_clear (&res, builder);
  dang_compile (expr->function.args[1], builder, &f, &res);
  dang_builder_add_jump_if_nonzero (builder, &res, redo_label);
  dang_compile_result_clear (&res, builder);
  dang_builder_pop_local_scope (builder);
  dang_builder_define_label (builder, break_label);
  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(do_while);
/* === mf-for.c === */

#define syntax_check__for  "AAAA"

#define annotate__for dang_mf_annotate__push_local_scope

static inline dang_boolean
is_void (DangExpr *expr)
{
  return expr->type == DANG_EXPR_TYPE_BAREWORD
      && strcmp (expr->bareword.name, "$void") == 0;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__for)
{
  DangCompileFlags void_flags = DANG_COMPILE_FLAGS_VOID;
  DangExpr *init, *condition, *advance, *body;
  DangLabelId redo_label, continue_label, break_label;
  dang_assert (flags->permit_void);

  init = expr->function.args[0];
  condition = expr->function.args[1];
  advance = expr->function.args[2];
  body = expr->function.args[3];

  dang_builder_push_local_scope (builder);

  if (!is_void (init))
    {
      dang_builder_push_tmp_scope (builder);
      dang_compile (init, builder, &void_flags, result);
      if (result->type == DANG_COMPILE_RESULT_ERROR)
        return;
      dang_compile_result_clear (result, builder);
      dang_builder_pop_tmp_scope (builder);
    }
  redo_label = dang_builder_start_scoped_label (builder, "$redo");
  continue_label = dang_builder_start_scoped_label (builder, "$continue");
  break_label = dang_builder_start_scoped_label (builder, "$break");
  dang_builder_define_label (builder, redo_label);
  if (!is_void (condition))
    {
      DangCompileFlags flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
      flags.permit_void = 0;
      dang_builder_push_tmp_scope (builder);
      dang_compile (condition, builder, &flags, result);
      if (result->type == DANG_COMPILE_RESULT_ERROR)
        return;
      dang_builder_add_jump_if_zero (builder, result, break_label);
      dang_compile_result_clear (result, builder);
      dang_builder_pop_tmp_scope (builder);
    }
  if (!is_void (body))
    {
      dang_builder_push_tmp_scope (builder);
      dang_compile (body, builder, &void_flags, result);
      if (result->type == DANG_COMPILE_RESULT_ERROR)
        return;
      dang_compile_result_clear (result, builder);
      dang_builder_pop_tmp_scope (builder);
    }
  dang_builder_define_label (builder, continue_label);
  if (!is_void (advance))
    {
      dang_builder_push_tmp_scope (builder);
      dang_compile (advance, builder, &void_flags, result);
      if (result->type == DANG_COMPILE_RESULT_ERROR)
        return;
      dang_compile_result_clear (result, builder);
      dang_builder_pop_tmp_scope (builder);
    }
  dang_builder_add_jump (builder, redo_label);
  dang_builder_define_label (builder, break_label);
  dang_builder_end_scoped_label (builder, break_label);
  dang_builder_end_scoped_label (builder, continue_label);
  dang_builder_end_scoped_label (builder, redo_label);
  dang_builder_pop_local_scope (builder);
  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(for);

#if 0
/* === mf-foreach.c === */

/* $foreach($bound_vars(a, b, c), container_expr, body_statement) */
#define syntax_check__foreach   "$bound_vars()AA"
#define syntax_check__bound_vars "*B"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE (annotate__foreach)
{
  DangExprTag *tag;
  DangValueType *type;
  unsigned n_args;
  unsigned cont_index;
  dang_var_table_push (var_table);
  if (!dang_expr_annotate_types (annotations, expr->function.args[1],
                                 imports, var_table, error))
    return FALSE;
  tag = dang_mf_get_tag (annotations, expr->function.args[1], var_table);
  dang_assert (tag != NULL);
  if (tag->tag_type != DANG_EXPR_TAG_VALUE)
    {
      dang_set_error (error, "expected value in foreach(), got %s ("DANG_CP_FORMAT")",
                      dang_expr_tag_type_name (tag->tag_type),
                      DANG_CP_EXPR_ARGS (expr->function.args[1]));
      return FALSE;
    }
  type = tag->info.value.type;

  /* Look-up the collection that has the appropriate number of parameters. */
  n_args = expr->function.args[0]->function.n_args;
  for (cont_index = 0; cont_index < type->internals.n_containers; cont_index++)
    if (type->internals.containers[cont_index]->n_params == n_args)
      break;
  if (cont_index == type->internals.n_containers)
    {
      if (cont_index == 0)
        dang_set_error (error, "type %s is not a container ("DANG_CP_FORMAT")",
                        type->full_name,
                        DANG_CP_EXPR_ARGS (expr->function.args[1]));
      else
        dang_set_error (error, "type %s was not a container with %u elements ("DANG_CP_FORMAT")",
                        type->full_name, n_args,
                        DANG_CP_EXPR_ARGS (expr->function.args[1]));
      return FALSE;
    }

  /* Annotate the bound vars with var_ids and types */
  for (i = 0; i < n_vars; i++)
    {
      DangExpr *decl_expr = expr->function.args[0]->function.args[i];
      const char *vname = decl_expr->bareword.name;
      DangValueType *vtype = type->internals.containers[cont_index]->params[i];
      DangVarId var_id = dang_var_table_alloc_local (var_table, vname,
                                                     decl_expr, vtype);
      dang_mf_annotate_local_var_id (annotations, decl_expr, var_id);
      dang_mf_annotate_value (annotations, decl_expr, vtype, FALSE, TRUE);
    }

  if (!dang_expr_annotate_types (annotations, expr->function.args[2],
                                 var_table, imports, error))
    return FALSE;
  dang_var_table_pop (var_table);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE (compile__foreach)
{
  DangExprCollection *coll_annot;
  DangCollectionSpec *collection;
  DangLabelId next_lab, end_lab;
  DangCompileResult container_res;
  DangCompileFlags container_flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;

  dang_builder_push_local_scope (builder);
  dang_builder_push_tmp_scope (builder);

  /* Compile container */
  dang_compile (builder, expr->function.args[1], &container_flags, &container_res);
  if (container_res.type == DANG_COMPILE_RESULT_ERROR)
    {
      *result = container_res;
      return;
    }

  /* Lock container var so it cannot be itself used as an lvalue. */
  dang_warning ("XXX: need way to guarantee that container isn't modified");

  coll_annot = dang_expr_get_annotation (annotations, expr,
                                         DANG_EXPR_ANNOTATION_COLLECTION);
  dang_assert (coll_annot != NULL);
  collection = coll_annot->collection_spec;
  iter_var = dang_builder_add_tmp (builder, collection->iter_var_type);
  collection->compile_init_iter (collection, &container_res, iter_var);

  next_lab = dang_builder_create_label (builder);
  dang_builder_define_label (builder, next_lab);
  end_lab = dang_builder_create_label (builder);

  /* Compile visit */
  collection->compile_robust_visit (collection, &container_res,
                                    iter_var, param_var_ids, end_lab);
  dang_compile (builder, expr->function.args[2], &dang_compile_flags_void, result);
  if (result->type == DANG_COMPILE_RESULT_ERROR)
    return;

  dang_builder_define_label (builder, end_lab);
  dang_builder_pop_tmp_scope (builder);
  dang_builder_pop_local_scope (builder);
}

DANG...BUILTIN_METAFUNCTION(foreach)
DANG...BUILTIN_METAFUNCTION__SYNTAX_CHECK_ONLY(bound_vars)
#endif


/* === mf-goto.c === */

#define syntax_check__goto    "B"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__goto)
{
  DANG_UNUSED (error);
  DANG_UNUSED (imports);
  DANG_UNUSED (var_table);
  dang_mf_annotate_statement (annotations, expr);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__goto)
{
  const char *name = expr->function.args[0]->bareword.name;
  DangLabelId id;
  DANG_UNUSED (flags);
  id = dang_builder_find_named_label (builder, name, &expr->any.code_position);
  dang_builder_add_jump (builder, id);
  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(goto);
/* === mf-if.c === */

#define syntax_check__if   "AA*A"

#define annotate__if dang_mf_annotate__push_local_scope

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__if)
{
  DangLabelId end_label, next;
  unsigned i;
  dang_boolean has_next = FALSE;

  DANG_UNUSED (flags);

  end_label = dang_builder_create_label (builder);

  dang_builder_push_local_scope (builder);

  for (i = 0; i < expr->function.n_args; i += 2)
    {
      DangCompileFlags flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
      unsigned body_index;
      if (has_next)
        {
          dang_builder_define_label (builder, next);
          has_next = FALSE;
        }
      if (i + 1 < expr->function.n_args)
        {
          dang_builder_push_tmp_scope (builder);
          dang_compile (expr->function.args[i], builder, &flags, result);
          if (result->type == DANG_COMPILE_RESULT_ERROR)
            return;
          next = dang_builder_create_label (builder);
          has_next = TRUE;
          dang_builder_add_jump_if_zero (builder, result, next);
          dang_builder_pop_tmp_scope (builder);
          body_index = i + 1;
          dang_compile_result_clear (result, builder);
        }
      else
        body_index = i;

      dang_builder_push_tmp_scope (builder);
      dang_compile (expr->function.args[body_index], builder,
                    &dang_compile_flags_void, result);
      if (result->type == DANG_COMPILE_RESULT_ERROR)
        return;
      dang_builder_pop_tmp_scope (builder);
      if (i + 2 < expr->function.n_args)
        dang_builder_add_jump (builder, end_label);
      dang_compile_result_clear (result, builder);
    }

  dang_builder_pop_local_scope (builder);
  if (has_next)
    dang_builder_define_label (builder, next);
  dang_builder_define_label (builder, end_label);
  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(if);
/* === mf-interpolated_string.c === */

#define syntax_check__interpolated_string "@"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__interpolated_string)
{
  unsigned i;
  for (i = 0; i < expr->function.n_args; i++)
    if (!dang_expr_annotate_types (annotations, expr->function.args[i], imports, var_table, error))
      return FALSE;
  dang_mf_annotate_value (annotations, expr, dang_value_type_string (), FALSE, TRUE);
  return TRUE;
}

static dang_boolean is_literal_string (DangExpr *expr)
{
  return expr->type == DANG_EXPR_TYPE_VALUE
      && expr->value.type == dang_value_type_string ();
}
static void
free_compile_result_array (unsigned n, DangCompileResult *results, DangBuilder *builder)
{
  unsigned i;
  for (i = 0; i < n; i++)
    dang_compile_result_clear (results + i, builder);
  dang_free (results);
}

/* $interpolated_string(...) */
/* plan:  for 0 arguments or 1 literal string, return a literal.
 *        Otherwise, compile to_string(arg) on the non-literal arguments.
 *        For 1 argument strings (e.g. "$i", just use the compilation result).
 *        For more than one argument, dang_compile_function_invocation()
 *        with the function dang_function_concat.
 */
static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__interpolated_string)
{
  DangCompileResult *pieces;
  DangVarId return_var_id;
  unsigned i;
  DangCompileFlags our_flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  DangCompileResult lvalue;
  DangFunction *func;
  DangCompileResult func_res;
  our_flags.permit_untyped = 0;
  our_flags.permit_void = 0;
  if (expr->function.n_args == 0
   || (expr->function.n_args == 1
      && is_literal_string (expr->function.args[0])))
    {
      DangString *str;
      if (expr->function.n_args == 0)
        str = dang_string_new ("");
      else
        {
          str = * (DangString **) expr->function.args[0]->value.value;
          if (str)
            str = dang_string_ref_copy (str);
        }
      dang_compile_result_init_literal (result, dang_value_type_string (), &str);
      dang_string_unref (str);
      dang_compile_obey_flags (builder, flags, result);
      return;
    }
  pieces = dang_new (DangCompileResult, expr->function.n_args);
  for (i = 0; i < expr->function.n_args; i++)
    {
      DangExpr *arg = expr->function.args[i];
      DangCompileResult *piece = pieces + i;
      if (arg->type == DANG_EXPR_TYPE_VALUE)
        {
          /* set argument to a literal */
          dang_compile_result_init_literal (piece, arg->value.type,
                                            arg->value.value);
        }
      else
        {
          /* compile expression */
          dang_compile (arg, builder, &our_flags, piece);
          if (piece->type == DANG_COMPILE_RESULT_ERROR)
            {
              dang_compile_result_set_error (result, &arg->any.code_position,
                                             "error compiling interpolated string expression (piece #%u): %s",
                                             i+1, piece->error.error->message);
              free_compile_result_array (i + 1, pieces, builder);
              return;
            }
        }
      if (piece->any.return_type != dang_value_type_string ())
        {
          DangFunction *to_str_func;
          DangMatchQuery query;
          DangMatchQueryElement element;
          DangCompileResult rv_info;
          const char *to_string_literal = "to_string";
          DangError *error = NULL;
          DangCompileResult func_name_res;

          /* compile to_string(...) */
          memset (&element, 0, sizeof (element));
          element.type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
          element.info.simple_input = piece->any.return_type;
          query.n_elements = 1;
          query.elements = &element;
          query.imports = builder->imports;
          to_str_func = dang_imports_lookup_function (builder->imports,
                                                      1, (char **) &to_string_literal,
                                                      &query, &error);
          if (to_str_func == NULL)
            {
              dang_compile_result_set_error (result, &arg->any.code_position,
                                             "no to_string() function for type %s: %s",
                                             piece->any.return_type->full_name, error->message);
              dang_error_unref (error);
              free_compile_result_array (i + 1, pieces, builder);
              return;
            }
          dang_compile_result_init_literal (&func_name_res,
                                            dang_value_type_function (to_str_func->base.sig),
                                            &to_str_func);
          dang_compile_result_init_stack (&rv_info, dang_value_type_string (), 
                         dang_builder_add_tmp (builder, dang_value_type_string ()),
                                          FALSE, TRUE, FALSE);
          dang_compile_function_invocation (&func_name_res, builder,
                                           &rv_info, 1, pieces + i);
          dang_compile_result_clear (&func_name_res, builder);
          dang_compile_result_clear (piece, builder);
          dang_compile_result_init_stack (piece, dang_value_type_string (),
                                          rv_info.stack.var_id,
                                          TRUE, FALSE, TRUE);
          dang_function_unref (to_str_func);
          dang_compile_result_clear (&rv_info, builder);
        }
    }
  if (expr->function.n_args == 1)
    {
      /* Return pieces[0] */
      *result = pieces[0];
      dang_compile_obey_flags (builder, flags, result);
      for (i = 1; i < expr->function.n_args; i++)
        dang_compile_result_clear (pieces + i, builder);
      dang_free (pieces);
      return;
    }

  return_var_id = dang_builder_add_tmp (builder, dang_value_type_string ());
  dang_compile_result_init_stack (&lvalue, dang_value_type_string (),
                                  return_var_id, FALSE, TRUE, FALSE);
  func = dang_function_concat_peek (expr->function.n_args);
  dang_compile_result_init_literal (&func_res,
                                    dang_value_type_function (func->base.sig),
                                    &func);
  dang_compile_function_invocation (&func_res, builder, &lvalue, expr->function.n_args, pieces);
  dang_compile_result_init_stack (result, dang_value_type_string (),
                                  return_var_id, TRUE,
                                  FALSE, TRUE);
  free_compile_result_array (expr->function.n_args, pieces, builder);
  dang_compile_result_clear (&func_res, builder);
  dang_compile_result_clear (&lvalue, builder);
  return;
}
DANG_BUILTIN_METAFUNCTION(interpolated_string);
/* === mf-invoke.c === */
/* $invoke(function, arg, arg...) */


#define syntax_check__invoke   "A@"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__invoke)
{
  unsigned i;
  DangFunction *function = NULL;
  DangValueMethod *method = NULL;
  DangExprTag *func_name_tag;
  uint8_t *is_out_params;
  DangSignature *sig;
  dang_boolean has_this_pointer = 0;
  dang_assert (expr->function.n_args > 0);
  is_out_params = dang_alloca ((expr->function.n_args + 7) / 8);
  memset (is_out_params, 0, (expr->function.n_args + 7) / 8);
#define SET_IS_OUT(param_index) \
        is_out_params[(param_index)/8] |= 1<<((param_index)%8)
#define IS_OUT(param_index) \
        ((is_out_params[(param_index)/8] & 1<<((param_index)%8)) != 0)

  for (i = 0; i < expr->function.n_args; i++)
    {
      if (i > 0
       && dang_expr_is_function (expr->function.args[i], "$out_param"))
        SET_IS_OUT (i - 1);
      if (!dang_expr_annotate_types (annotations, expr->function.args[i], imports, var_table, error))
        return FALSE;
    }

  func_name_tag = dang_mf_get_tag (annotations, expr->function.args[0], var_table);
  dang_assert (func_name_tag != NULL);


  switch (func_name_tag->tag_type)
    {
    case DANG_EXPR_TAG_VALUE:
      if (!dang_value_type_is_function (func_name_tag->info.value.type))
        {
          dang_set_error (error, "called object is not a function (is a %s) ("DANG_CP_FORMAT")",
                          func_name_tag->info.value.type->full_name,
                          DANG_CP_ARGS (expr->function.args[0]->any.code_position));
          return FALSE;
        }
      sig = ((DangValueTypeFunction*)func_name_tag->info.value.type)->sig;
      break;

    case DANG_EXPR_TAG_METHOD:
    case DANG_EXPR_TAG_FUNCTION_FAMILY:
      {
        /* Fill up a MatchQuery to try to get a concrete function out of the family. */
        unsigned n_elements = expr->function.n_args - 1;
        DangMatchQuery query;
        DangMatchQueryElement *elements = dang_newa (DangMatchQueryElement, n_elements);
        memset (elements, 0, sizeof (DangMatchQueryElement) * n_elements);
        for (i = 0; i < n_elements; i++)
          {
            DangExprTag *tag = dang_mf_get_tag (annotations, expr->function.args[i+1], var_table);
            dang_boolean is_out = IS_OUT (i);
            if (tag->tag_type == DANG_EXPR_TAG_VALUE)
              {
                if (is_out)
                  {
                    elements[i].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT;
                    elements[i].info.simple_output = tag->info.value.type;
                  }
                else
                  {
                    elements[i].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
                    elements[i].info.simple_input = tag->info.value.type;
                  }
              }
            else if (tag->tag_type == DANG_EXPR_TAG_FUNCTION_FAMILY)
              {
                DangFunctionFamily *ff = tag->info.ff.family;
                DangFunction *func;
                func = dang_function_family_is_single (ff);
                if (func != NULL)
                  {
                    tag->info.ff.function = func;
                    elements[i].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
                    elements[i].info.simple_input = dang_value_type_function (func->base.sig);
                  }
                else
                  {
                    elements[i].type = DANG_MATCH_QUERY_ELEMENT_FUNCTION_FAMILY;
                    elements[i].info.function_family = ff;
                  }
              }
            else if (tag->tag_type == DANG_EXPR_TAG_UNTYPED_FUNCTION)
              {
                elements[i].type = DANG_MATCH_QUERY_ELEMENT_UNTYPED_FUNCTION;
                elements[i].info.untyped_function = tag->info.untyped_function;
              }
            else if (tag->tag_type == DANG_EXPR_TAG_CLOSURE)
              {
                elements[i].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
                elements[i].info.simple_input = tag->info.closure.function_type;
              }
            else
              {
                dang_set_error (error, "unexpected tag: expected value (got %s) (%s:%u)",
                                dang_expr_tag_type_name (tag->tag_type),
                                DANG_CP_EXPR_ARGS (expr->function.args[i+1]));
                return FALSE;
              }
          }
        query.n_elements = n_elements;
        query.elements = elements;
        query.imports = imports;

        if (func_name_tag->tag_type == DANG_EXPR_TAG_FUNCTION_FAMILY)
          {
            function = dang_function_family_try (func_name_tag->info.ff.family, &query, error);
            if (function == NULL)
              {
                if (error)
                  dang_error_add_prefix (*error, "at "DANG_CP_FORMAT, 
                                         DANG_CP_EXPR_ARGS (expr));
                return FALSE;
              }
            func_name_tag->info.ff.function = function;     /* takes reference */
            sig = function->base.sig;
          }
        else if (func_name_tag->tag_type == DANG_EXPR_TAG_METHOD)
          {
            DangValueElement *element_out;
            unsigned index_out;
            DangValueType *method_type_out;
            if (!dang_value_type_find_method (func_name_tag->info.method.object_type,
                                              func_name_tag->info.method.name,
                                              func_name_tag->info.method.has_object,
                                              &query,
                                              &method_type_out, &element_out, &index_out, error))
              {
                if (error && *error == NULL)
                  {
                    DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
                    dang_match_query_dump (&query, &buf);
                    dang_set_error (error,
                                    "error finding method %s%s in object of type %s ("DANG_CP_FORMAT")",
                                    func_name_tag->info.method.name,
                                    buf.str,
                                    func_name_tag->info.method.object_type->full_name,
                                    DANG_CP_ARGS (expr->function.args[0]->any.code_position));
                    dang_free (buf.str);
                  }
                return FALSE;
              }

            /* sig and function will include 'this' for
               non-static methods; we need to keep track
               since we will coerce our partially typed arguments
               with 'sig', they have to line up. */
            func_name_tag->info.method.method_type = method_type_out;
            func_name_tag->info.method.method_element = element_out;
            func_name_tag->info.method.index = index_out;
            method = ((DangValueMethod*)element_out->info.methods.data)
                   + index_out;
            sig = method->sig;
            has_this_pointer = (method->flags & DANG_METHOD_STATIC) ? 0 : 1;
          }
        else
          dang_assert_not_reached ();
      }
      break;


    default:
      dang_set_error (error, "unexpected tag as first arg to '$invoke' (got %s) (%s:%u)",
                      dang_expr_tag_type_name (func_name_tag->tag_type),
                      DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }

  dang_assert (sig->n_params == expr->function.n_args - 1 + has_this_pointer);

  /* propagate type info to arguments?? */
  //...

  /* Resolve function families in the argument list
     to be simply functions. */
  for (i = 1; i < expr->function.n_args; i++)
    {
      DangExprTag *tag = dang_mf_get_tag (annotations, expr->function.args[i], var_table);
      if (tag->tag_type == DANG_EXPR_TAG_FUNCTION_FAMILY
       && tag->info.ff.function == NULL)
        {
          DangValueType *ptype = sig->params[i-1+has_this_pointer].type;
          DangSignature *psig;
          DangMatchQuery pmq;
          DangMatchQueryElement *pelements;
          unsigned j;
          dang_assert (dang_value_type_is_function (ptype));
          psig = ((DangValueTypeFunction*)ptype)->sig;
          pelements = dang_new (DangMatchQueryElement, psig->n_params);
          for (j = 0; j < psig->n_params; j++)
            {
              if (psig->params[j].dir == DANG_FUNCTION_PARAM_IN)
                {
                  pelements[j].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT;
                  pelements[j].info.simple_input = psig->params[j].type;
                }
              else
                {
                  pelements[j].type = DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT;
                  pelements[j].info.simple_output = psig->params[j].type;
                }
            }
          pmq.n_elements = psig->n_params;
          pmq.elements = pelements;
          tag->info.ff.function = dang_function_family_try (tag->info.ff.family, &pmq, error);
          dang_free (pelements);
          if (tag->info.ff.function == NULL)
            {
              dang_error_add_prefix (*error, "at "DANG_CP_FORMAT, 
                                     DANG_CP_EXPR_ARGS (expr));
              return FALSE;
            }
        }
      else if (tag->tag_type == DANG_EXPR_TAG_UNTYPED_FUNCTION)
        {
          DangValueType *ptype = sig->params[i-1+has_this_pointer].type;
          DangSignature *psig;
          DangUntypedFunction *uf = tag->info.untyped_function;
          DangValueType *ltype, *rtype;
          dang_assert (dang_value_type_is_function (ptype));
          psig = ((DangValueTypeFunction*)ptype)->sig;
          if (!dang_untyped_function_make_stub (uf, psig->params, error))
            return FALSE;

          /* Check for return-type agreement. */
          ltype = psig->return_type;
          rtype = uf->func->base.sig->return_type;
          if (!dang_value_type_is_autocast (ltype, rtype))
            {
              dang_set_error (error,
                              "untyped function: inferred type of %s, needed %s ("DANG_CP_FORMAT")",
                              rtype ? rtype->full_name : "(void)",
                              ltype ? ltype->full_name : "(void)",
                              DANG_CP_EXPR_ARGS (expr->function.args[i]));
              return FALSE;
            }
        }
      else if (tag->tag_type == DANG_EXPR_TAG_VALUE)
        {
          dang_boolean ok;
          switch (sig->params[i-1+has_this_pointer].dir)
            {
            case DANG_FUNCTION_PARAM_IN:
              ok = tag->info.value.is_rvalue;
              break;
            case DANG_FUNCTION_PARAM_OUT:
              ok = tag->info.value.is_lvalue;
              break;
            case DANG_FUNCTION_PARAM_INOUT:
              ok = tag->info.value.is_lvalue && tag->info.value.is_rvalue;
              break;
            }
          if (!ok)
            {
              dang_set_error (error, "%s parameter #%u is %s ("DANG_CP_FORMAT")",
                              dang_function_param_dir_name (sig->params[i-1+has_this_pointer].dir),
                              i,
                              tag->info.value.is_lvalue ? "write-only"
                                                        : "read-only",
                              DANG_CP_EXPR_ARGS (expr->function.args[i]));
              return FALSE;
            }
        }
    }

  if (sig->return_type == NULL
   || sig->return_type == dang_value_type_void ())
    dang_mf_annotate_statement (annotations, expr);
  else
    dang_mf_annotate_value (annotations, expr, sig->return_type, FALSE, TRUE);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE (compile__invoke)
{
  DangCompileResult *results;
  DangCompileResult ret_res_buf;
  DangCompileResult *ret_res;
  unsigned n_params = expr->function.n_args - 1;
  unsigned i;
  DangCompileFlags subflags_in = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  DangCompileFlags subflags_inout = DANG_COMPILE_FLAGS_LRVALUE_PERMISSIVE;
  DangCompileFlags subflags_out = DANG_COMPILE_FLAGS_LVALUE_PERMISSIVE;
  DangCompileFlags fnflags = DANG_COMPILE_FLAGS_RVALUE_RESTRICTIVE;
  DangCompileResult func_name_res;
  DangSignature *sig;
  DangExprTag *tag;
  DangCompileResult object_res;
  dang_boolean need_implicit_this = 0;

  tag = dang_expr_get_annotation (builder->annotations, expr->function.args[0], DANG_EXPR_ANNOTATION_TAG);
  dang_assert (tag != NULL);
  if (tag->tag_type == DANG_EXPR_TAG_METHOD)
    {
      DangValueMethod *method = tag->info.method.method_element->info.methods.data;
      DangError *error = NULL;
      dang_assert (method != NULL);
      method += tag->info.method.index;
      if (!dang_builder_check_method_access (builder, 
                                                      tag->info.method.method_type,
                                                      method->flags,
                                                      &error))
        {
          const char *name = expr->function.args[0]->function.args[1]->bareword.name;
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "method access %s.%s() denied: %s",
                                         tag->info.method.object_type->full_name,
                                         name, error->message);
          dang_error_unref (error);
          return;
        }
      if (tag->info.method.has_object)
        {
          /* Compile object. */
          dang_compile (expr->function.args[0]->function.args[0],
                        builder, &subflags_in, &object_res);
          need_implicit_this = 1;

          /* invoke method compiler */
          if (method->func != NULL)
            {
              dang_compile_result_init_literal (&func_name_res,
                                                method->method_func_type,
                                                &method->func);
            }
          else
            {
              DangFunction *f = method->get_func;
              dang_assert (f->base.sig->n_params == 1
                        && f->base.sig->params[0].dir == DANG_FUNCTION_PARAM_IN
                        && dang_value_type_is_object (f->base.sig->params[0].type));
              dang_compile_result_init_stack (&func_name_res, f->base.sig->return_type,
                                              dang_builder_add_tmp (builder,
                                                                             f->base.sig->return_type),
                                              FALSE, TRUE, FALSE);
              dang_compile_literal_function_invocation (f, builder, &func_name_res,
                                                        1, &object_res);
            }
        }
      else
        {
          if (method->func != NULL)
            {
              dang_compile_result_init_literal (&func_name_res,
                                                method->method_func_type,
                                                &method->func);
            }
          else
            {
              DangFunction *f = method->get_func;
              dang_assert (f->base.sig->n_params == 0);
              dang_compile_result_init_stack (&func_name_res, f->base.sig->return_type,
                                              dang_builder_add_tmp (builder,
                                                                             f->base.sig->return_type),
                                              FALSE, TRUE, FALSE);
              dang_compile_literal_function_invocation (f, builder, &func_name_res,
                                                        0, NULL);
            }
        }
      sig = method->sig;
    }
  else
    {
      if (dang_expr_is_function (expr->function.args[0], "$operator_new"))
        {
          DangExpr *type_expr = expr->function.args[0]->function.args[0];
          DangValueType *type = *(DangValueType**)type_expr->value.value;
          dang_object_note_method_stubs (type, builder->function->stub.cc);
        }
      fnflags.permit_literal = 1;
      dang_compile (expr->function.args[0], builder, &fnflags, &func_name_res);
      if (func_name_res.type == DANG_COMPILE_RESULT_ERROR)
        {
          *result = func_name_res;
          return;
        }

      dang_assert (dang_value_type_is_function (func_name_res.any.return_type));
      sig = ((DangValueTypeFunction*)func_name_res.any.return_type)->sig;
    }

  dang_assert (sig->n_params == n_params + need_implicit_this);
  results = dang_new (DangCompileResult, sig->n_params);
  if (need_implicit_this)
    {
      /* Compile; use result as results[0] */
      results[0] = object_res;
    }
  for (i = 0; i < n_params; i++)
    {
      DangExpr *arg = expr->function.args[i+1];
      unsigned target_param = i + need_implicit_this;
      DangCompileFlags *f;
      if (dang_expr_is_function (arg, "$out_param"))
        {
          dang_assert (arg->function.n_args == 1);
          arg = arg->function.args[0];

          if (sig->params[i].dir == DANG_FUNCTION_PARAM_IN)
            {
              dang_compile_result_set_error (result, &expr->any.code_position,
                                             "got '&' on input parameter");
              return;
            }
        }
      else
        {
          if (sig->params[i].dir != DANG_FUNCTION_PARAM_IN)
            {
              const char *n = dang_function_param_dir_name (sig->params[i].dir);
              dang_compile_result_set_error (result, &expr->any.code_position,
                                             "need '&' on %s parameter", n);
              return;
            }
        }
      switch (sig->params[i].dir)
        {
        case DANG_FUNCTION_PARAM_IN: f = &subflags_in; break;
        case DANG_FUNCTION_PARAM_OUT: f = &subflags_out; break;
        case DANG_FUNCTION_PARAM_INOUT: f = &subflags_inout; break;
        default: dang_assert_not_reached ();
        }

      dang_compile (arg, builder, f, results + target_param);
      if (results[target_param].type == DANG_COMPILE_RESULT_ERROR)
        {
          *result = results[target_param];
          return;
        }
    }

  if (sig->return_type == NULL
   || sig->return_type == dang_value_type_void ())
    {
      ret_res = NULL;
    }
  else
    {
      DangValueType *rettype = sig->return_type;
      ret_res = &ret_res_buf;
      dang_compile_result_init_stack (ret_res, rettype,
                                      dang_builder_add_tmp (builder, rettype),
                                      FALSE, TRUE, FALSE);
    }
  dang_compile_function_invocation (&func_name_res, builder,
                                    ret_res, sig->n_params, results);
  if (ret_res)
    {
      *result = *ret_res;
    }
  else
    {
      dang_compile_result_init_void (result);
    }
  for (i = 0; i < n_params + need_implicit_this; i++)
    dang_compile_result_clear (results + i, builder);
  dang_free (results);

  if (result->type == DANG_COMPILE_RESULT_STACK)
    {
      result->any.is_lvalue = FALSE;
      result->any.is_rvalue = TRUE;
      result->stack.was_initialized = TRUE;
    }
  if (result->type != DANG_COMPILE_RESULT_ERROR)
    dang_compile_obey_flags (builder, flags, result);
  dang_compile_result_clear (&func_name_res, builder);
}

DANG_BUILTIN_METAFUNCTION(invoke);
/* === mf-label.c === */

#define syntax_check__label    "B"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__label)
{
  DANG_UNUSED (error);
  DANG_UNUSED (var_table);
  DANG_UNUSED (imports);
  dang_mf_annotate_statement (annotations, expr);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__label)
{
  const char *name = expr->function.args[0]->bareword.name;
  DangLabelId id;
  DANG_UNUSED (flags);
  id = dang_builder_find_named_label (builder, name, &expr->any.code_position);
  if (dang_builder_is_label_defined (builder, id))
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "label %s already defined",
                                     name);
      return;
    }
  dang_builder_define_label (builder, id);
  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(label);

//////#define syntax_check__module  "B*B"
//////static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__module)
//////{
//////  DANG_UNUSED (var_table);
//////  DANG_UNUSED (expr);
//////  DANG_UNUSED (error);
//////  return TRUE;
//////}
//////static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(compile__module)
//////{
//////  char **words = dang_newa (char *, expr->function.n_args);
//////  for (i = 0; i < expr->function.n_args; i++)
//////    {
//////      ...
//////    }
//////  if (builder->imports->required_module_directive)
//////    {
//////      unsigned n_expected;
//////      char **expected = builder->imports->required_module_directive;
//////      for (i = 0; expected[i]; i++)
//////        ;
//////      n_expected = i;
//////      for (i = 0; i < expr->function.n_args; i++)
//////        {
//////          if (expected[i] == NULL || strcmp (expected[i], words[i]) != 0)
//////            break;
//////        }
//////      if (i < expr->function.n_args || i < n_expected)
//////        {
//////          char *exp = dang_util_join_with_dot (n_expected, expected);
//////          char *act = dang_util_join_with_dot (expr->function.n_args, words);
//////          dang_compile_result_set_error (result, &expr->any.code_position,
//////                                         "mismatched module directive: expected %s, got %s",
//////                                         exp, act);
//////          dang_free (exp);
//////          dang_free (act);
//////          return;
//////        }
//////    }
//////  else if (!builder->imports->permit_module_directive)
//////    {
//////      dang_compile_result_set_error (result, &expr->any.code_position,
//////                                     "module directive not allowed here");
//////      return;
//////    }
//////
//////  /* Create new 'imports' */
//////  ...
//////
//////  return TRUE;
//////}

/* === mf-operator_assign_logical.c === */
/* Implement assign-logical-or and assign-logical-and */

#define syntax_check__operator_assign_logical_or "AA"
#define syntax_check__operator_assign_logical_and "AA"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__assign_logical)
{
  DangExprTag *left_tag, *right_tag;
  if (!dang_expr_annotate_types (annotations, expr->function.args[0], imports, var_table, error)
   || !dang_expr_annotate_types (annotations, expr->function.args[1], imports, var_table, error))
    return FALSE;
  left_tag = dang_expr_get_annotation (annotations, expr->function.args[0], DANG_EXPR_ANNOTATION_TAG);
  right_tag = dang_expr_get_annotation (annotations, expr->function.args[1], DANG_EXPR_ANNOTATION_TAG);
  dang_assert (left_tag != NULL);
  dang_assert (right_tag != NULL);
  if (left_tag->tag_type != DANG_EXPR_TAG_VALUE)
    {
      dang_set_error (error, "left-hand side of logical-operator assign was not a value, was a %s ("DANG_CP_FORMAT")",
                      dang_expr_tag_type_name (left_tag->tag_type), DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }
  if (right_tag->tag_type != DANG_EXPR_TAG_VALUE)
    {
      dang_set_error (error, "right-hand side of logical-operator assign was not a value, was a %s ("DANG_CP_FORMAT")",
                      dang_expr_tag_type_name (left_tag->tag_type), DANG_CP_EXPR_ARGS (expr->function.args[1]));
      return FALSE;
    }
  if (!left_tag->info.value.is_rvalue
   || !left_tag->info.value.is_lvalue)
    {
      dang_set_error (error, "left-hand side of logical-operator assign was not %s ("DANG_CP_FORMAT")",
                      left_tag->info.value.is_rvalue ? "writable" : "readable", DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }
  if (!right_tag->info.value.is_rvalue)
    {
      dang_set_error (error, "right-hand side of logical-operator assign was not readable ("DANG_CP_FORMAT")",
                      DANG_CP_EXPR_ARGS (expr->function.args[1]));
      return FALSE;
    }
  if (!dang_value_type_is_autocast (left_tag->info.value.type, right_tag->info.value.type))
    {
      dang_set_error (error, "type-mismatch in logical-operator assign (%s v %s) ("DANG_CP_FORMAT")",
                      left_tag->info.value.type->full_name,
                      right_tag->info.value.type->full_name,
                      DANG_CP_EXPR_ARGS (expr));
      return FALSE;
    }

  dang_mf_annotate_statement (annotations, expr);
  return TRUE;
}

#define annotate__operator_assign_logical_or   annotate__assign_logical
#define annotate__operator_assign_logical_and  annotate__assign_logical


static void
compile__assign_logical  (DangExpr              *expr,
                          DangBuilder   *builder,
                          dang_boolean           jump_if_zero,
                          DangCompileFlags      *flags,
                          DangCompileResult     *result)
{
  DangCompileResult lvalue, rvalue;
  DangCompileFlags lflags = DANG_COMPILE_FLAGS_LRVALUE_PERMISSIVE;
  DangCompileFlags rflags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  DangLabelId target;
  dang_assert (flags->permit_void);

  dang_builder_push_tmp_scope (builder);

  /* Compile lvalue */
  dang_compile (expr->function.args[0], builder, &lflags, &lvalue);
  if (lvalue.type == DANG_COMPILE_RESULT_ERROR)
    {
      *result = lvalue;
      return;
    }

  /* Jump if zero/nonzero (depending on whether we are implementing and/or (resp)) */
  target = dang_builder_create_label (builder);
  dang_builder_add_conditional_jump (builder, &lvalue, jump_if_zero, target);

  /* Compile rvalue */
  dang_compile (expr->function.args[1], builder, &rflags, &rvalue);
  if (lvalue.type == DANG_COMPILE_RESULT_ERROR)
    {
      *result = rvalue;
      dang_compile_result_clear (&lvalue, builder);
      return;
    }

  /* Assign lvalue from rvalue */
  dang_builder_add_assign (builder, &lvalue, &rvalue);

  /* Set jump target */
  dang_builder_define_label (builder, target);

  dang_builder_pop_tmp_scope (builder);
  dang_compile_result_clear (&lvalue, builder);
  dang_compile_result_clear (&rvalue, builder);
  dang_compile_result_init_void (result);
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE (compile__operator_assign_logical_or)
{
  compile__assign_logical (expr, builder, FALSE, flags, result);
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE (compile__operator_assign_logical_and)
{
  compile__assign_logical (expr, builder, TRUE, flags, result);
}

DANG_BUILTIN_METAFUNCTION (operator_assign_logical_or);
DANG_BUILTIN_METAFUNCTION (operator_assign_logical_and);
/* === mf-operator_dot.c === */
/* $operator_dot(whatever, member) */


#define syntax_check__operator_dot   "AB|TB"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__operator_dot)
{
  const char *member_name;
  DangExprTag *container_tag;
  member_name = expr->function.args[1]->bareword.name;

  if (!dang_expr_annotate_types (annotations, expr->function.args[0], imports, var_table, error))
    return FALSE;

  container_tag = dang_mf_get_tag (annotations, expr->function.args[0], var_table);
  if (container_tag == NULL)
    return FALSE;
  dang_assert (container_tag != NULL);

  switch (container_tag->tag_type)
    {
    case DANG_EXPR_TAG_VALUE:
      {
        DangValueType *type = container_tag->info.value.type;
        dang_boolean is_lvalue = container_tag->info.value.is_lvalue;
        dang_boolean is_rvalue = container_tag->info.value.is_rvalue;
        DangValueElement *element;
        DangValueMember *member;
        if (type == NULL)
          return FALSE;
        element = dang_value_type_lookup_element (type, member_name, TRUE, NULL);
        if (element == NULL)
          {
            dang_set_error (error, "no member %s of type '%s' (%s:%u)",
                            member_name, type->full_name,
                            DANG_CP_EXPR_ARGS (expr));
            return FALSE;
          }
        if (element->element_type == DANG_VALUE_ELEMENT_TYPE_METHOD)
          {
            dang_mf_annotate_method (annotations, expr, TRUE, type, element);
            return TRUE;
          }
        if (element->element_type != DANG_VALUE_ELEMENT_TYPE_MEMBER)
          {
            dang_set_error (error, "non-member %s %s of type '%s' (%s:%u)",
                            dang_value_element_type_name (element->element_type),
                            member_name, type->full_name,
                            DANG_CP_EXPR_ARGS (expr));
            return FALSE;
          }
        member = &element->info.member;
        if (member->type == DANG_VALUE_MEMBER_TYPE_SIMPLE
            && member->info.simple.dereference)
          is_lvalue = is_rvalue = TRUE;
        /* XXX: this would be a good time to check,
         * but we test in compile__operator_dot,
         * so not checking here is safe. */
        //if ((member->flags & DANG_MEMBER_PUBLIC_READABLE) == 0)
        //  is_rvalue = FALSE;
        //if ((member->flags & DANG_MEMBER_PUBLIC_WRITABLE) == 0)
        //  is_lvalue = FALSE;
        dang_mf_annotate_value (annotations, expr, member->member_type, is_lvalue, is_rvalue);
        dang_mf_annotate_member (annotations, expr, member);
        return TRUE;
      }
    case DANG_EXPR_TAG_NAMESPACE:
      {
        DangNamespaceSymbol *sym = dang_namespace_lookup (container_tag->info.ns, member_name);
        if (sym == NULL)
          {
            dang_set_error (error, "no symbol %s in namespace %s (%s:%u)",
                            member_name, container_tag->info.ns->full_name,
                            DANG_CP_EXPR_ARGS (expr));
            return FALSE;
          }
        dang_mf_annotate_from_namespace_symbol (annotations, expr, container_tag->info.ns, sym);
        return TRUE;
      }
    case DANG_EXPR_TAG_TYPE:
      {
        DangValueType *type = * (DangValueType **) expr->function.args[0]->value.value;
        DangValueElement *element;
        dang_assert (type->magic == DANG_VALUE_TYPE_MAGIC);
        element = dang_value_type_lookup_element (type, member_name, TRUE, NULL);
        if (element == NULL)
          {
            dang_set_error (error, "no member %s of type '%s' (%s:%u)",
                            member_name, type->full_name,
                            DANG_CP_EXPR_ARGS (expr));
            return FALSE;
          }
        if (element->element_type == DANG_VALUE_ELEMENT_TYPE_METHOD)
          {
            dang_mf_annotate_method (annotations, expr, FALSE, type, element);
            return TRUE;
          }
        if (element->element_type != DANG_VALUE_ELEMENT_TYPE_MEMBER)
          {
            dang_set_error (error, "non-member %s %s of type '%s' (%s:%u)",
                            dang_value_element_type_name (element->element_type),
                            member_name, type->full_name,
                            DANG_CP_EXPR_ARGS (expr));
            return FALSE;
          }
        DangValueMember *member;
        dang_boolean is_lvalue = TRUE, is_rvalue = TRUE;
        member = &element->info.member;
        /* XXX: this would be a good time to check,
         * but we test in compile__operator_dot,
         * so not checking here is safe. */
        //if ((member->flags & DANG_MEMBER_PUBLIC_READABLE) == 0)
        //  is_rvalue = FALSE;
        //if ((member->flags & DANG_MEMBER_PUBLIC_WRITABLE) == 0)
        //  is_lvalue = FALSE;
        dang_mf_annotate_value (annotations, expr, member->member_type, is_lvalue, is_rvalue);
        dang_mf_annotate_member (annotations, expr, member);

        dang_die ("static members not supported");
        return TRUE;
      }
    case DANG_EXPR_TAG_FUNCTION_FAMILY:
    case DANG_EXPR_TAG_STATEMENT:
    case DANG_EXPR_TAG_UNTYPED_FUNCTION:
    case DANG_EXPR_TAG_CLOSURE:
    case DANG_EXPR_TAG_METHOD:
    case DANG_EXPR_TAG_OBJECT_DEFINE:
      dang_set_error (error, "expression of tag %s not allowed as first arg to $operator_dot (%s:%u)",
                      dang_expr_tag_type_name (container_tag->tag_type),
                      DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }
  dang_assert_not_reached ();
  return FALSE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__operator_dot)
{
  DangCompileResult cur_res;
  DangValueElement *element;
  DangValueType *base_type;
  DangExprMember *member_info;
  DangCompileFlags lflags = *flags;

  member_info = dang_expr_get_annotation (builder->annotations, expr,
                                          DANG_EXPR_ANNOTATION_MEMBER);
  if (member_info != NULL)
    {
      if (member_info->dereference)
        {
          lflags.must_be_lvalue = FALSE;
          lflags.must_be_rvalue = TRUE;
        }
      else
        {
          if (lflags.must_be_lvalue)
            lflags.must_be_rvalue = TRUE;
        }
      dang_compile (expr->function.args[0], builder, &lflags, &cur_res);
      if (cur_res.type == DANG_COMPILE_RESULT_ERROR)
        {
          *result = cur_res;
          return;
        }
      element = dang_value_type_lookup_element (cur_res.any.return_type, expr->function.args[1]->bareword.name, TRUE, &base_type);
      if (element == NULL)
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "attempt to access non-member of %s",
                                         cur_res.any.return_type->full_name,
                                         expr->function.args[1]->bareword.name);
          return;
        }
      if (element->element_type != DANG_VALUE_ELEMENT_TYPE_MEMBER)
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "attempt to access %s %s of %s as though it were a member",
                                         dang_value_element_type_name (element->element_type),
                                         expr->function.args[1]->bareword.name,
                                         cur_res.any.return_type->full_name);
          return;
        }
      dang_compile_member_access (builder, &cur_res, base_type, element->name, &element->info.member,
                                  flags, result);
      return;
    }
  else
    {
      DangExprNamespaceSymbol *nsym;
      nsym = dang_expr_get_annotation (builder->annotations,
                                       expr,
                                       DANG_EXPR_ANNOTATION_NAMESPACE_SYMBOL);
      if (nsym != NULL && nsym->symbol->type == DANG_NAMESPACE_SYMBOL_GLOBAL)
        {
          dang_compile_result_init_global (result,
                                           nsym->symbol->info.global.type,
                                           nsym->ns,
                                           nsym->symbol->info.global.offset,
                                           !nsym->symbol->info.global.is_constant,
                                           TRUE);
          dang_compile_obey_flags (builder, flags, result);
          return;
        }
    }
  dang_compile_result_set_error (result, &expr->any.code_position,
                                 "operator_dot on non-value or namespace global");
}

DANG_BUILTIN_METAFUNCTION(operator_dot);
/* === mf-operator_index.c === */
/* $operator_index() */


#define syntax_check__operator_index "AA@"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__operator_index)
{
  DangExprTag *container_tag;
  DangValueType *container_type;
  unsigned i;
  DangValueType **index_types;
  DangValueIndexInfo *ii;
  if (!dang_mf_annotate__recurse (annotations, expr, imports, var_table, error))
    return FALSE;

  /* is the first arg a container (with an index method)? */
  container_tag = dang_mf_get_tag (annotations, expr->function.args[0], var_table);
  dang_assert (container_tag != NULL);
  dang_assert (container_tag->tag_type == DANG_EXPR_TAG_VALUE);
  container_type = container_tag->info.value.type;
  if (container_type->internals.index_infos == NULL)
    {
      dang_set_error (error, "cannot index (using []) a non-container type (%s)",
                      container_type->full_name);
      return FALSE;
    }

  index_types = dang_newa (DangValueType *, expr->function.n_args - 1);
  for (i = 1; i < expr->function.n_args; i++)
    {
      DangExprTag *tag = dang_mf_get_tag (annotations, expr->function.args[i], var_table);
      dang_assert (tag != NULL);
      dang_assert (tag->tag_type == DANG_EXPR_TAG_VALUE);
      index_types[i-1] = tag->info.value.type;
    }

  /* are the subsequent terms uint or int? */
  for (ii = container_type->internals.index_infos; ii; ii = ii->next)
    if (ii->n_indices == expr->function.n_args - 1)
      {
        for (i = 0; i < ii->n_indices; i++)
          if (!dang_value_type_is_autocast (ii->indices[i], index_types[i]))
            break;
        if (i == ii->n_indices)
          break;
      }
  if (ii == NULL)
    {
      DangStringBuffer buf = DANG_STRING_BUFFER_INIT;
      dang_string_buffer_printf (&buf, "no index operation with type(s) %s",
                                 index_types[0]->full_name);
      for (i = 2; i < expr->function.n_args; i++)
        dang_string_buffer_printf (&buf, ",%s", index_types[i-1]->full_name);
      dang_string_buffer_printf (&buf, "found for container type %s",
                                 container_type->full_name);
      dang_set_error (error, "%s ("DANG_CP_FORMAT")", buf.str,
                      DANG_CP_EXPR_ARGS (expr));
      dang_free (buf.str);
      return FALSE;
    }
  dang_mf_annotate_index_info (annotations, expr, ii);
  dang_mf_annotate_value (annotations, expr, ii->element_type,
                          container_tag->info.value.is_lvalue,
                          container_tag->info.value.is_rvalue);
  return TRUE;
}

typedef struct _IndexLValueCallbackData IndexLValueCallbackData;
struct _IndexLValueCallbackData
{
  DangCompileResult array;
  DangValueType *element_type;
  DangValueIndexInfo *index_info;
  DangInsnValue element;
  DangInsnValue *indices;
};

static void
index_lvalue_callback (DangCompileResult   *result,
                       DangBuilder *builder)
{
  IndexLValueCallbackData *cbdata = result->stack.callback_data;
  DangInsn insn;
  dang_insn_init (&insn, DANG_INSN_TYPE_INDEX);

  dang_insn_value_from_compile_result (&insn.index.container, &cbdata->array);
  insn.index.index_info = cbdata->index_info;
  insn.index.indices = cbdata->indices;
  insn.index.element = cbdata->element;
  insn.index.is_set = TRUE;
  dang_builder_add_insn (builder, &insn);
  
  if (cbdata->array.type == DANG_COMPILE_RESULT_STACK)
    {
      if (cbdata->array.stack.lvalue_callback != NULL)
        cbdata->array.stack.lvalue_callback (&cbdata->array, builder);
    }
}

static void
free_index_lvalue_callback_data (void *data, DangBuilder *builder)
{
  IndexLValueCallbackData *cbdata = data;
  dang_compile_result_clear (&cbdata->array, builder);
  dang_free (cbdata);
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__operator_index)
{
  DangCompileResult container_res;
  DangCompileResult index_res;
  DangCompileFlags array_flags = *flags;
  DangCompileFlags index_flags = DANG_COMPILE_FLAGS_RVALUE_RESTRICTIVE;
  DangInsnValue *index_values = dang_new (DangInsnValue, expr->function.n_args - 1);
  unsigned i;
  DangCompileResult hack;
  DangExprIndexInfo *eii = dang_expr_get_annotation (builder->annotations, expr,
                                                     DANG_EXPR_ANNOTATION_INDEX_INFO);
  DangExprTag *tag = dang_expr_get_annotation (builder->annotations,
                                               expr, DANG_EXPR_ANNOTATION_TAG);
  DangValueType *element_type;
  dang_boolean must_copy_index_values = FALSE;
  dang_assert (eii != NULL);
  dang_assert (tag != NULL);
  dang_assert (tag->tag_type == DANG_EXPR_TAG_VALUE);

  /* A result just used to accumulate locks */
  dang_compile_result_init_void (&hack);
  for (i = 1; i < expr->function.n_args; i++)
    {
      dang_compile (expr->function.args[i], builder, &index_flags, &index_res);
      if (index_res.type == DANG_COMPILE_RESULT_ERROR)
        {
          *result = index_res;
          return;
        }
      dang_assert (index_res.type == DANG_COMPILE_RESULT_STACK);
      index_values[i-1].location = DANG_INSN_LOCATION_STACK;
      index_values[i-1].var = index_res.stack.var_id;
      dang_assert (index_res.stack.lvalue_callback == NULL);
      dang_compile_result_steal_locks (&hack, &index_res);
      dang_compile_result_clear (&index_res, builder);
    }

  array_flags.permit_pointer = 1;
  array_flags.permit_literal = !flags->must_be_lvalue;
  if (flags->must_be_lvalue)
    array_flags.must_be_rvalue = 1;
  dang_compile (expr->function.args[0], builder, &array_flags, &container_res);
  if (container_res.type == DANG_COMPILE_RESULT_ERROR)
    {
      *result = container_res;
      return;
    }
  element_type = tag->info.value.type;
  DangVarId res_var_id;
  res_var_id = dang_builder_add_tmp (builder, element_type);

  if (flags->must_be_rvalue)
    {
      DangInsn insn;
      dang_insn_init (&insn, DANG_INSN_TYPE_INDEX);
      dang_insn_value_from_compile_result (&insn.index.container, &container_res);
      insn.index.indices = index_values;
      insn.index.element.location = DANG_INSN_LOCATION_STACK;
      insn.index.element.var = res_var_id;
      insn.index.is_set = FALSE;
      insn.index.index_info = eii->index_info;
      must_copy_index_values = TRUE;
      dang_builder_note_var_create (builder, res_var_id);
      dang_builder_add_insn (builder, &insn);
    }
  dang_compile_result_init_stack (result,
                                  tag->info.value.type,
                                  res_var_id,
                                  flags->must_be_rvalue,
                                  flags->must_be_lvalue,
                                  flags->must_be_rvalue);
  if (!flags->permit_uninitialized)
    dang_compile_result_force_initialize (builder, result);
  dang_compile_result_steal_locks (result, &container_res);
  dang_compile_result_steal_locks (result, &hack);
  if (flags->must_be_lvalue)
    {
      IndexLValueCallbackData *cbdata;
      if (eii->index_info->set == NULL)
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "%s is not mutable",
                                         eii->index_info->owner->full_name);
          return;
        }
      cbdata = dang_malloc (sizeof (IndexLValueCallbackData));
      if (must_copy_index_values)
        cbdata->indices = copy_values (eii->index_info->n_indices,
                                       index_values);
      else
        {
          cbdata->indices = index_values;
          must_copy_index_values = TRUE;
        }
      cbdata->array = container_res;
      cbdata->element.location = DANG_INSN_LOCATION_STACK;
      cbdata->element.var = result->stack.var_id;
      cbdata->element_type = element_type;
      cbdata->index_info = eii->index_info;

      result->stack.lvalue_callback = index_lvalue_callback;
      result->stack.callback_data = cbdata;
      result->stack.callback_data_destroy = free_index_lvalue_callback_data;
    }
  else
    dang_compile_result_clear (&container_res, builder);

  /* if the index is lvalue or rvalue, we will set this flag
     indicating that we have given up ownership of
     the indices.  But, surely the value is lvalue or rvalue or both. */
  dang_assert (must_copy_index_values);

  dang_compile_result_clear (&hack, builder);
}
DANG_BUILTIN_METAFUNCTION(operator_index);
/* === mf-operator_logical.c === */

/* Implement $operator_logical_and and $operator_logical_or */
DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__logical)
{
  DangExprTag *lhs_tag, *rhs_tag;
  unsigned i;
  const char *op = expr->function.name[18] == 'a' ? "&&" : "||";
  dang_assert (expr->function.n_args >= 2);
  if (!dang_expr_annotate_types (annotations, expr->function.args[0], imports, var_table, error)
   || !dang_expr_annotate_types (annotations, expr->function.args[1], imports, var_table, error))
    return FALSE;
  lhs_tag = dang_mf_get_tag (annotations, expr->function.args[0], var_table);
  dang_assert (lhs_tag);

  for (i = 1; i < expr->function.n_args; i++)
    {
      rhs_tag = dang_mf_get_tag (annotations, expr->function.args[1], var_table);
      dang_assert (rhs_tag);

      if (lhs_tag->tag_type != DANG_EXPR_TAG_VALUE
       || rhs_tag->tag_type != DANG_EXPR_TAG_VALUE
       || lhs_tag->info.value.type == NULL
       || rhs_tag->info.value.type == NULL)
        {
          dang_set_error (error, 
                          "arguments to %s must be typed value (%s:%u)",
                          op, DANG_CP_EXPR_ARGS (expr));
          return FALSE;
        }
      if (lhs_tag->info.value.type != rhs_tag->info.value.type)
        {
          dang_set_error (error, 
                          "%s: type mismatch (%s v %s) (%s:%u)", op,
                          lhs_tag->info.value.type->full_name,
                          rhs_tag->info.value.type->full_name,
                          DANG_CP_EXPR_ARGS (expr));
          return FALSE;
        }
    }
  dang_mf_annotate_value (annotations, expr, lhs_tag->info.value.type, FALSE, TRUE);
  return TRUE;
}

static void
compile_generic_logical_op (DangExpr              *expr,
                            DangBuilder   *builder,
                            dang_boolean   jump_to_end_if_zero,
                            DangCompileResult     *result)
{
  DangVarId var_id;
  DangLabelId label_id;
  unsigned i;

  /* allocate result */
  /* TODO: see if it's possible to re-use result for our return-value,
     e.g. if it's a temporary or something.  */
  var_id = dang_builder_add_tmp (builder, NULL);

  label_id = dang_builder_create_label (builder);

  for (i = 0; i < expr->function.n_args; i++)
    {
      DangCompileFlags subflags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
      DangCompileResult subresult;
      subflags.permit_void = 0;
      subflags.permit_untyped = 0;
      subflags.permit_uninitialized = 0;
      dang_builder_push_tmp_scope (builder);
      dang_compile (expr->function.args[i], builder, &subflags, &subresult);
      if (subresult.type == DANG_COMPILE_RESULT_ERROR)
        {
          *result = subresult;
          return;
        }
      dang_assert (subresult.type != DANG_COMPILE_RESULT_VOID);
      if (i == 0)
        {
          dang_builder_bind_local_type (builder, var_id, subresult.any.return_type);
          dang_compile_result_init_stack (result, subresult.any.return_type, var_id,
                                          FALSE, TRUE, FALSE);
        }
      else
        {
          if (subresult.any.return_type != result->any.return_type)
            {
              DangValueType *orig_type = result->any.return_type;
              dang_compile_result_set_error (result, &expr->function.args[i]->any.code_position,
                                             "cannot cast from %s to %s",
                                             subresult.any.return_type->full_name,
                                             orig_type->full_name);
              dang_compile_result_clear (&subresult, builder);
              return;
            }
          result->stack.was_initialized = TRUE;
        }
      dang_builder_add_assign (builder, result, &subresult);
      dang_builder_pop_tmp_scope (builder);
      if (i + 1 != expr->function.n_args)
        dang_builder_add_conditional_jump (builder, result,
                                       jump_to_end_if_zero, label_id);
      dang_compile_result_clear (&subresult, builder);
    }
  dang_builder_define_label (builder, label_id);

  result->any.is_lvalue = FALSE;
  result->any.is_rvalue = TRUE;
}

#define syntax_check__operator_logical_and "@"

#define annotate__operator_logical_and annotate__logical

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__operator_logical_and)
{
  DANG_UNUSED (flags);
  compile_generic_logical_op (expr, builder, TRUE, result);
}

DANG_BUILTIN_METAFUNCTION(operator_logical_and);

#define syntax_check__operator_logical_or "@"
#define annotate__operator_logical_or annotate__logical

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__operator_logical_or)
{
  DANG_UNUSED (flags);
  compile_generic_logical_op (expr, builder, FALSE, result);
}

DANG_BUILTIN_METAFUNCTION(operator_logical_or);
/* === mf-operator_new.c === */

#define syntax_check__operator_new "T|TB"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__operator_new)
{
  DangValueType *type = * (DangValueType **) expr->function.args[0]->value.value;
  const char *name;
  DangFunctionFamily *ctor;
  DANG_UNUSED (imports);
  DANG_UNUSED (var_table);
  dang_assert (expr->function.args[0]->type == DANG_EXPR_TYPE_VALUE);
  dang_assert (expr->function.args[0]->value.type == dang_value_type_type());
  if (expr->function.n_args == 1)
    name = NULL;
  else
    {
      dang_assert (expr->function.args[1]->type == DANG_EXPR_TYPE_BAREWORD);
      name = expr->function.args[1]->bareword.name;
    }

  ctor = dang_value_type_get_ctor (type, name);
  if (ctor == NULL)
    {
      if (name == NULL)
        {
          dang_set_error (error, "no unnamed constructors for type %s",
                          type->full_name);
          return FALSE;
        }
      else
        {
          dang_set_error (error, "no constructors named '%s' for type %s",
                          name, type->full_name);
          return FALSE;
        }
    }
  dang_mf_annotate_function_family (annotations, expr, ctor);
  return TRUE;
}

#define compile__operator_new  NULL       /* always trapped by $invoke() */

DANG_BUILTIN_METAFUNCTION(operator_new);
/* === mf-out_param.c === */

#define syntax_check__out_param "A"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__out_param)
{
  DangExprTag *tag;
  if (!dang_expr_annotate_types (annotations, expr->function.args[0], imports, var_table, error))
    return FALSE;
  tag = dang_expr_get_annotation (annotations, expr->function.args[0], DANG_EXPR_ANNOTATION_TAG);
  if (tag->tag_type != DANG_EXPR_TAG_VALUE)
    {
      dang_set_error (error, "expected value inside $out_param");
      return FALSE;
    }
  if (!tag->info.value.is_lvalue)
    {
      dang_set_error (error, "expected lvalue inside $out_param");
      return FALSE;
    }
  dang_mf_annotate_value (annotations, expr, tag->info.value.type, tag->info.value.is_lvalue, tag->info.value.is_rvalue);
  return TRUE;
}
#define compile__out_param  NULL                /* always handled by invoke */

DANG_BUILTIN_METAFUNCTION(out_param);
/* === mf-redo.c === */

#define syntax_check__redo   "|I"
#define annotate__redo dang_mf_annotate__none
#define compile__redo dang_mf_compile__generic_scoped_label_jump

DANG_BUILTIN_METAFUNCTION(redo);
/* === mf-return.c === */

#define syntax_check__return "|A"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__return)
{
  DangExprTag *tag;
  DangValueType *old_type;
  DangValueType *rv_type;
  if (expr->function.n_args == 0)
    return TRUE;
  if (!var_table->has_rv)
    {
      dang_set_error (error, "argument to return in function without a return-value at "DANG_CP_FORMAT,
                      DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }

  dang_var_table_push (var_table);
  if (!dang_expr_annotate_types (annotations, expr->function.args[0],
                                 imports, var_table, error))
    return FALSE;
  tag = dang_expr_get_annotation (annotations, expr->function.args[0],
                                  DANG_EXPR_ANNOTATION_TAG);
  dang_assert (tag != NULL);
  dang_var_table_pop (var_table);
  switch (tag->tag_type)
    {
    case DANG_EXPR_TAG_VALUE:
      rv_type = tag->info.value.type;
      break;
    case DANG_EXPR_TAG_CLOSURE:
      rv_type = tag->info.closure.function_type;
      break;
    default:
      dang_set_error (error,
                      "argument to return is not a value (is a %s) at "DANG_CP_FORMAT,
                      dang_expr_tag_type_name (tag->tag_type),
                      DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }
  old_type = dang_var_table_get_return_type (var_table);
  if (old_type != NULL)
    {
      if (rv_type != old_type)
        {
          dang_set_error (error, "type mismatch (%s v %s) attempting to return a value at "DANG_CP_FORMAT,
                          rv_type->full_name, old_type->full_name,
                          DANG_CP_EXPR_ARGS (expr->function.args[0]));
          return FALSE;
        }
    }
  else
    {
      dang_var_table_set_type (var_table,
                               dang_var_table_get_return_var_id (var_table),
                               expr, rv_type);
    }
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__return)
{
  DANG_UNUSED (flags);
  if (expr->function.n_args != 0)
    {
      DangCompileFlags flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
      DangExpr *retval_expr = expr->function.args[0];
      DangCompileResult rv, subresult;
      dang_compile (retval_expr, builder, &flags, &subresult);
      if (subresult.type == DANG_COMPILE_RESULT_ERROR)
        {
          *result = subresult;
          return;
        }
      dang_compile_result_init_stack (&rv, dang_builder_get_var_type (builder, 0), 0,
                                      TRUE, TRUE, FALSE);
      dang_builder_add_assign (builder, &rv, &subresult);
      dang_compile_result_clear (&subresult, builder);
      dang_compile_result_clear (&rv, builder);
    }
  dang_builder_add_return (builder);
  dang_compile_result_init_void (result);
}

DANG_BUILTIN_METAFUNCTION(return);
/* === mf-statement_list.c === */

/* $statement_list(...) */
#define syntax_check__statement_list "@"

#define annotate__statement_list  dang_mf_annotate__push_local_scope

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE (compile__statement_list)
{
  unsigned i;
  dang_assert (flags->permit_void);
  dang_compile_result_init_void (result);
  dang_builder_push_local_scope (builder);
  for (i = 0; i < expr->function.n_args; i++)
    {
      DangCompileResult tmp;
      dang_builder_push_tmp_scope (builder);
      dang_compile (expr->function.args[i], builder, flags, &tmp);
      if (tmp.type == DANG_COMPILE_RESULT_ERROR)
        {
          *result = tmp;
          dang_builder_pop_tmp_scope (builder);
          dang_builder_pop_local_scope (builder);
          return;
        }
      dang_compile_result_clear (&tmp, builder);
      dang_builder_pop_tmp_scope (builder);
    }
  dang_builder_pop_local_scope (builder);
}
DANG_BUILTIN_METAFUNCTION(statement_list);
/* === mf-tensor.c === */
#include "config.h"

#define syntax_check__tensor   "A@"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__tensor)
{
  DangExprTensorSizes *new = NULL;
  unsigned i;
  dang_boolean may_fold;
  DangExprTag *elt_tag;
  DangValueType *elt_type;

  for (i = 0; i < expr->function.n_args; i++)
    if (!dang_expr_annotate_types (annotations, expr->function.args[i], imports, var_table, error))
      return FALSE;

  /* Check that all the types match. */
  may_fold = !expr->function.args[0]->any.had_parentheses;
  elt_tag = dang_mf_get_tag (annotations, expr->function.args[0], var_table);
  dang_assert (elt_tag->tag_type == DANG_EXPR_TAG_VALUE);
  elt_type = elt_tag->info.value.type;
  for (i = 1; i < expr->function.n_args; i++)
    {
      if (expr->function.args[i]->any.had_parentheses)
        may_fold = FALSE;
      elt_tag = dang_mf_get_tag (annotations, expr->function.args[i], var_table);
      dang_assert (elt_tag->tag_type == DANG_EXPR_TAG_VALUE);
      if (elt_tag->info.value.type != elt_type)
        {
          dang_set_error (error, "type-mismatch %s v %s in element %u of tensor",
                          elt_type->full_name, elt_tag->info.value.type->full_name, i);
          return FALSE;
        }
    }
  if (may_fold && dang_value_type_is_tensor (elt_type))
    {
      DangExprTensorSizes *first_sizes, *next_sizes;
      unsigned j;

      /* Go back and get size annotations for each tensor;
         if they all match, fold into a larger tensor */
      first_sizes = dang_expr_get_annotation (annotations,
                                             expr->function.args[0],
                                             DANG_EXPR_ANNOTATION_TENSOR_SIZES);
      for (i = 1; i < expr->function.n_args; i++)
        {
          next_sizes = dang_expr_get_annotation (annotations,
                                                expr->function.args[i],
                                                DANG_EXPR_ANNOTATION_TENSOR_SIZES);
          dang_assert (first_sizes->rank == next_sizes->rank);
          for (j = 0; j < first_sizes->rank; j++)
            if (first_sizes->sizes[j] != next_sizes->sizes[j])
              break;
          if (j < first_sizes->rank)
            break;
        }
      if (i == expr->function.n_args)
        {
          /* Add a rank+1 size annotations */
          new = dang_malloc (sizeof (DangExprTensorSizes)
                           + sizeof(unsigned) * first_sizes->rank);
          new->rank = first_sizes->rank + 1;
          memcpy (new->sizes + 1, first_sizes->sizes, sizeof (unsigned) * first_sizes->rank);
          new->sizes[0] = i;
          elt_type = first_sizes->elt_type;
        }
    }
  if (new == NULL)
    {
      /* Add a vector annotation */
      new = dang_new (DangExprTensorSizes, 1);
      new->rank = 1;
      new->sizes[0] = expr->function.n_args;
    }
  new->elt_type = elt_type;
  dang_expr_annotation_init (annotations, expr, DANG_EXPR_ANNOTATION_TENSOR_SIZES, new);

  dang_mf_annotate_value (annotations, expr, dang_value_type_tensor (elt_type, new->rank), FALSE, TRUE);
  return TRUE;
}

static dang_boolean
advance_indices (unsigned *indices,
                 DangExprTensorSizes *sizes)
{
  unsigned i = sizes->rank - 1;
  for (;;)
    {
      if (++(indices[i]) < sizes->sizes[i])
        return TRUE;
      indices[i] = 0;
      if (i == 0)
        return FALSE;
      i--;
    }
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__tensor)
{
  DangExpr *top;
  DangCompileFlags subflags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  DangCompileResult lvalue, rvalue;
  unsigned rank;
  unsigned total_tensor_size;
  unsigned i;
  DangCompileResult subresult;
  DangValueType *elt_type;
  size_t elt_size;
  DangInsn insn;
  DangValueType *tensor_type;
  DangVarId tensor_var_id;
  DangVarId data_var_id;
  DangExprTensorSizes *tensor_sizes;
  unsigned tensor_data_offset;
  unsigned *indices;

  if (flags->must_be_lvalue)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "$tensor does not yield an lvalue");
      return;
    }

  top = expr->function.args[0];
  tensor_sizes = dang_expr_get_annotation (builder->annotations,
                                           expr, DANG_EXPR_ANNOTATION_TENSOR_SIZES);
  rank = tensor_sizes->rank;

  total_tensor_size = 1;
  for (i = 0; i < rank; i++)
    total_tensor_size *= tensor_sizes->sizes[i];

  /* Construct the "make_tensor_data", which is a sequence
     of run-length-encoded steps */
  elt_type = tensor_sizes->elt_type;
  elt_size = elt_type->sizeof_instance;
  tensor_type = dang_value_type_tensor (elt_type, rank);

  /* Reserve space for the 'make_tensor' step,
     as well as the stack space for the tensor. */
  tensor_var_id = dang_builder_add_tmp (builder, tensor_type);
  dang_builder_note_var_create (builder, tensor_var_id);

  dang_insn_init (&insn, DANG_INSN_TYPE_NEW_TENSOR);
  insn.new_tensor.target = tensor_var_id;
  insn.new_tensor.elt_type = elt_type;
  insn.new_tensor.rank = tensor_sizes->rank;
  insn.new_tensor.dims = dang_new (unsigned, tensor_sizes->rank);
  memcpy (insn.new_tensor.dims, tensor_sizes->sizes, sizeof(unsigned) * tensor_sizes->rank);
  insn.new_tensor.total_size = total_tensor_size * tensor_sizes->elt_type->sizeof_instance;
  dang_builder_add_insn (builder, &insn);

  /* Create a pointer at the data section of the tensor */
  dang_builder_push_tmp_scope (builder);
  data_var_id = dang_builder_add_tmp (builder, dang_value_type_reserved_pointer ());
  dang_compile_result_init_pointer (&rvalue,
                                    dang_value_type_reserved_pointer (),
                                    tensor_var_id,
                                    offsetof (DangTensor, data),
                                    FALSE, TRUE);
  dang_compile_result_init_stack (&lvalue,
                                  dang_value_type_reserved_pointer (),
                                  data_var_id, FALSE, TRUE, FALSE);
  dang_builder_add_assign (builder, &lvalue, &rvalue);
  dang_compile_result_clear (&lvalue, builder);
  dang_compile_result_clear (&rvalue, builder);

  /* walk through tensor data, computing run-lengths */
  indices = dang_newa (unsigned, tensor_sizes->rank);
  for (i = 0; i < tensor_sizes->rank; i++)
    indices[i] = 0;
  tensor_data_offset = 0;
  for (;;)
    {
      /* Find expr */
      DangExpr *e = expr;
      for (i = 0; i < tensor_sizes->rank; i++)
        {
          dang_assert (dang_expr_is_function (e, "$tensor"));
          e = e->function.args[indices[i]];
        }
      if (e->type == DANG_EXPR_TYPE_VALUE)
        {
          if (!dang_util_is_zero (e->value.value, e->value.type->sizeof_instance))
            {
              dang_compile_result_init_literal (&rvalue, e->value.type, e->value.value);
              dang_compile_result_init_pointer (&lvalue,
                                                elt_type,
                                                data_var_id,
                                                tensor_data_offset,
                                                TRUE, FALSE);
              dang_builder_add_assign (builder, &lvalue, &rvalue);
              dang_compile_result_clear (&lvalue, builder);
              dang_compile_result_clear (&rvalue, builder);
            }
        }
      else
        {
          /* Compute the value */
          dang_builder_push_tmp_scope (builder);
          dang_compile (e, builder, &subflags, &subresult);
          if (subresult.type == DANG_COMPILE_RESULT_ERROR)
            {
              *result = subresult;
              return;
            }

          /* do assignment */
          dang_compile_result_init_pointer (&lvalue,
                                            elt_type,
                                            data_var_id,
                                            tensor_data_offset,
                                            TRUE, FALSE);
          dang_builder_add_assign (builder, &lvalue, &subresult);
          dang_compile_result_clear (&subresult, builder);
          dang_compile_result_clear (&lvalue, builder);
          dang_builder_pop_tmp_scope (builder);
        }
      tensor_data_offset += elt_size;
      if (!advance_indices (indices, tensor_sizes))
        break;
    }
  dang_builder_pop_tmp_scope (builder);


  dang_compile_result_init_stack (result,
                                  tensor_type,       /* return-type */
                                  tensor_var_id,
                                  TRUE,              /* was-initialized */
                                  FALSE, TRUE);      /* is-lvalue, is-rvalue */
}

DANG_BUILTIN_METAFUNCTION(tensor);

/* === mf-tree === */
#include "config.h"

#define syntax_check__ctree            "$ctree_entry()*$ctree_entry()"
#define syntax_check__ctree_entry      "AA"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__ctree)
{
  unsigned i;
  DangValueType *key_type, *value_type;

  for (i = 0; i < expr->function.n_args; i++)
    {
      DangExpr *kv = expr->function.args[i];
      DangValueType *kt, *vt;
      DangExprTag *tag;
      dang_assert (kv->function.n_args == 2);
      if (!dang_expr_annotate_types (annotations, kv->function.args[0], imports, var_table, error)
       || !dang_expr_annotate_types (annotations, kv->function.args[1], imports, var_table, error))
        return FALSE;

      tag = dang_mf_get_tag (annotations, kv->function.args[0], var_table);
      dang_assert (tag);
      dang_assert (tag->tag_type == DANG_EXPR_TAG_VALUE);
      kt = tag->info.value.type;
      tag = dang_mf_get_tag (annotations, kv->function.args[1], var_table);
      dang_assert (tag);
      dang_assert (tag->tag_type == DANG_EXPR_TAG_VALUE);
      vt = tag->info.value.type;
      if (i == 0)
        {
          key_type = kt;
          value_type = vt;
        }
      else
        {
          /* check is compatible */
          if (!dang_value_type_is_autocast (key_type, kt))
            {
              dang_set_error (error, "type-mismatch %s v %s in key %u of tree constructor",
                              key_type->full_name, kt->full_name, i + 1);
              return FALSE;
            }
          if (!dang_value_type_is_autocast (value_type, vt))
            {
              dang_set_error (error, "type-mismatch %s v %s in value %u of tree constructor",
                              value_type->full_name, vt->full_name, i + 1);
              return FALSE;
            }
        }
    }

  dang_mf_annotate_value (annotations, expr, dang_value_type_constant_tree (key_type, value_type), FALSE, TRUE);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__ctree)
{
  unsigned i;
  DangInsn insn;
  DangValueType *tree_type;
  DangValueType *key_type, *value_type;
  DangVarId tree_var_id;
  DangExprTag *tag;
  DangValueTreeTypes *tt;

  if (flags->must_be_lvalue)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "$tree does not yield an lvalue");
      return;
    }

  tag = dang_expr_get_annotation (builder->annotations, expr, DANG_EXPR_ANNOTATION_TAG);
  tree_type = tag->info.value.type;
  tt = ((DangValueTypeTree *) tree_type)->owner;
  key_type = tt->key;
  value_type = tt->value;

  /* Reserve space for the 'make_tree' step,
     as well as the stack space for the tree. */
  tree_var_id = dang_builder_add_tmp (builder, tree_type);
  dang_builder_note_var_create (builder, tree_var_id);

  /* create an empty (pseudo-constant) tree */
  dang_insn_init (&insn, DANG_INSN_TYPE_NEW_CONSTANT_TREE);
  insn.new_constant_tree.target = tree_var_id;
  insn.new_constant_tree.key_type = key_type;
  insn.new_constant_tree.value_type = value_type;
  dang_builder_add_insn (builder, &insn);

  for (i = 0; i < expr->function.n_args; i++)
    {
      /* Find expr */
      DangExpr *key_expr = expr->function.args[i]->function.args[0];
      DangExpr *value_expr = expr->function.args[i]->function.args[1];
      if (key_expr->type != DANG_EXPR_TYPE_VALUE
       || value_expr->type != DANG_EXPR_TYPE_VALUE)
        {
          DangCompileResult params[3];
          DangCompileResult subrv;
          dang_builder_push_tmp_scope (builder);

          /* compile key */
          dang_compile (key_expr, builder,
                        &dang_compile_flags_rvalue_restrictive, &params[1]);
          if (params[1].type == DANG_COMPILE_RESULT_ERROR)
            {
              *result = params[1];
              return;
            }
          dang_assert (params[1].type == DANG_COMPILE_RESULT_STACK);

          /* compile value */
          dang_compile (key_expr, builder,
                        &dang_compile_flags_rvalue_restrictive, &params[2]);
          if (params[2].type == DANG_COMPILE_RESULT_ERROR)
            {
              dang_compile_result_clear (params + 1, builder);
              *result = params[2];
              return;
            }
          dang_assert (params[2].type == DANG_COMPILE_RESULT_STACK);

          dang_compile_result_init_stack (params + 0,
                                          tree_type,
                                          tree_var_id,
                                          TRUE, FALSE, TRUE);
          dang_compile_result_init_void (&subrv);
          dang_compile_literal_function_invocation (tt->constant_tree_set,
                                                    builder,
                                                    &subrv,
                                                    3,
                                                    params);

          dang_compile_result_clear (&params[0], builder);
          dang_compile_result_clear (&params[1], builder);
          dang_compile_result_clear (&params[2], builder);
          dang_builder_pop_tmp_scope (builder);
        }
    }

  dang_compile_result_init_stack (result,
                                  tree_type,       /* return-type */
                                  tree_var_id,
                                  TRUE,              /* was-initialized */
                                  FALSE, TRUE);      /* is-lvalue, is-rvalue */
}

DANG_BUILTIN_METAFUNCTION__SYNTAX_CHECK_ONLY(ctree_entry);
DANG_BUILTIN_METAFUNCTION(ctree);

/* === mf-type_dot.c === */

#define syntax_check__type_dot   "TB"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__type_dot)
{
  DangValueType *left = * (DangValueType**) expr->function.args[0]->value.value;
  const char *name = expr->function.args[1]->bareword.name;
  DANG_UNUSED (imports);
  DANG_UNUSED (var_table);
  if (dang_value_type_is_enum (left))
    {
      DangEnumValue *v = dang_enum_lookup_value_by_name ((DangValueTypeEnum*)left,
                                                         name);
      if (v == NULL)
        {
          dang_set_error (error, "no enum-value '%s' in '%s' obtainable ("DANG_CP_FORMAT")",
                          name, left->full_name, DANG_CP_EXPR_ARGS (expr));
          return FALSE;
        }
      dang_mf_annotate_value (annotations, expr, left, FALSE, TRUE);
      return TRUE;
    }
  else
    {
      dang_set_error (error, "no members of '%s' obtainable ("DANG_CP_FORMAT")",
                      left->full_name, DANG_CP_EXPR_ARGS (expr));
      return FALSE;
    }
  return TRUE;
}
static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__type_dot)
{
  DangValueType *left = * (DangValueType**) expr->function.args[0]->value.value;
  const char *name = expr->function.args[1]->bareword.name;
  if (dang_value_type_is_enum (left))
    {
      DangEnumValue *v = dang_enum_lookup_value_by_name ((DangValueTypeEnum*)left, name);
      const void *val;
      uint8_t val1;
      uint16_t val2;
      uint32_t val4;
      if (v == NULL)
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "no enum-value '%s' in '%s' obtainable",
                                         name, left->full_name);
          return;
        }
      switch (left->sizeof_instance)
        {
        case 1: val1 = v->code; val = &val1; break;
        case 2: val2 = v->code; val = &val2; break;
        case 4: val4 = v->code; val = &val4; break;
        default: dang_assert_not_reached ();
        }
      dang_compile_result_init_literal (result, left, val);
      dang_compile_obey_flags (builder, flags, result);
      return;
    }
  else
    dang_assert_not_reached ();
}

DANG_BUILTIN_METAFUNCTION(type_dot);
/* === mf-untyped_function.c === */
/* $untyped_function(params, body) */

#define syntax_check__untyped_function  "$parameters()A"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__untyped_function)
{
  DangUntypedFunction *uf;
  unsigned i;
  DangVarId *closure_params_var_ids;

  DANG_UNUSED (error);


  uf = dang_new (DangUntypedFunction, 1);
  uf->n_params = expr->function.args[0]->function.n_args;
  uf->param_names = dang_new (char *, uf->n_params);
  for (i = 0; i < uf->n_params; i++)
    {
      DangExpr *arg = expr->function.args[0]->function.args[i];
      dang_assert (arg->type == DANG_EXPR_TYPE_BAREWORD);
      uf->param_names[i] = dang_strdup (arg->bareword.name);
    }
  uf->body = dang_expr_ref (expr->function.args[1]);
  uf->imports = dang_imports_ref (imports);
  uf->func = NULL;
  uf->rejects = NULL;
  uf->failures = NULL;

  /* Gather closure params */
  dang_mf_gather_closure_params (uf->n_params, uf->param_names, var_table, expr, 
                                 &uf->n_closure_params, &closure_params_var_ids);
  uf->closure_params = dang_new (DangUntypedFunctionImplicitParam,
                                 uf->n_closure_params);
  for (i = 0; i < uf->n_closure_params; i++)
    {
      DangVarId id = closure_params_var_ids[i];
      uf->closure_params[i].var_id = id;
      uf->closure_params[i].type = dang_var_table_get_type (var_table, id);
      uf->closure_params[i].name = dang_strdup (dang_var_table_get_name (var_table, id));
    }
  dang_free (closure_params_var_ids);
  dang_mf_annotate_untyped_function (annotations, expr, uf);
  return TRUE;
}


DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__untyped_function)
{
  DangExprTag *tag;
  DangUntypedFunction *untyped;
  DangFunction *underlying;
  DangVarId *closure_var_ids;
  DangCompileResult func_name_res;
  unsigned i;

  tag = dang_expr_get_annotation (builder->annotations, expr, DANG_EXPR_ANNOTATION_TAG);
  dang_assert (tag);
  dang_assert (tag->tag_type == DANG_EXPR_TAG_UNTYPED_FUNCTION);

  untyped = tag->info.untyped_function;
  underlying = untyped->func;
  dang_assert (underlying != NULL);


  dang_compile_result_init_literal (&func_name_res,
                                    dang_value_type_function (underlying->base.sig),
                                    &underlying);

  closure_var_ids = dang_new (DangVarId, untyped->n_closure_params);
  for (i = 0; i < untyped->n_closure_params; i++)
    closure_var_ids[i] = untyped->closure_params[i].var_id;
  dang_compile_create_closure (builder,
                               &expr->any.code_position,
                               &func_name_res,
                               untyped->n_closure_params,
                               closure_var_ids,
                               result);
  dang_free (closure_var_ids);
  dang_compile_result_clear (&func_name_res, builder);
  dang_compile_obey_flags (builder, flags, result);
  return;
}

DANG_BUILTIN_METAFUNCTION(untyped_function);

#define syntax_check__parameters    "*B"
#define annotate__parameters        NULL
#define compile__parameters         NULL
DANG_BUILTIN_METAFUNCTION(parameters);

/* === mf-var_decl.c === */

/* $var_decl(name) */
/* $var_decl(type, name) */

#define syntax_check__var_decl   "B|TB"

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__var_decl)
{
  DangExpr *name_expr;
  const char *name;
  DangVarId var_id;
  DangValueType *type = NULL;

  name_expr = expr->function.args[expr->function.n_args - 1];
  name = name_expr->bareword.name;

  if (expr->function.n_args == 2)
    {
      DangExprTag *tag;
      if (!dang_expr_annotate_types (annotations, expr->function.args[0], imports, var_table, error))
        return FALSE;
      tag = dang_expr_get_annotation (annotations, expr->function.args[0], DANG_EXPR_ANNOTATION_TAG);
      dang_assert (tag != NULL);
      if (tag->tag_type != DANG_EXPR_TAG_TYPE)
        {
          dang_set_error (error, "expected type as first arg to '$var_decl' (got %s) (%s:%u)",
                          dang_expr_tag_type_name (tag->tag_type),
                          DANG_CP_EXPR_ARGS (expr->function.args[0]));
          return FALSE;
        }
      type = tag->info.type;
      if (type->internals.is_templated)
        {
          dang_set_error (error, "non-instantiable type %s given in variable declarations for %s ("DANG_CP_FORMAT")",
                          type->full_name, name,
                          DANG_CP_EXPR_ARGS (expr->function.args[0]));
          return FALSE;
        }
    }

  var_id = dang_var_table_alloc_local (var_table, name, expr, type);
  if (var_id == DANG_VAR_ID_INVALID)
    {
      dang_set_error (error, "variable '%s' already defined in '$var_decl' (%s:%u)",
                      expr->function.args[0]->bareword.name,
                      DANG_CP_EXPR_ARGS (expr->function.args[0]));
      return FALSE;
    }
  
  dang_mf_annotate_local_var_id (annotations, expr, var_id);
  dang_mf_annotate_value (annotations, expr, type, TRUE, TRUE);
  return TRUE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__var_decl)
{
  unsigned n_args = expr->function.n_args;
  const char *name;
  DangValueType *type;
  DangVarId var_id;
  DangInsn insn;
  DangExprVarId *var_id_annot;

  name = expr->function.args[n_args-1]->bareword.name;

  var_id_annot = dang_expr_get_annotation (builder->annotations, expr, DANG_EXPR_ANNOTATION_VAR_ID);
  dang_assert (var_id_annot != NULL);
  var_id = var_id_annot->var_id;
  if (n_args == 2)
    {
      dang_assert (expr->function.args[0]->type == DANG_EXPR_TYPE_VALUE);
      dang_assert (expr->function.args[0]->value.type == dang_value_type_type ());
      type = * (DangValueType**) expr->function.args[0]->value.value;
      dang_builder_add_local (builder, name, var_id, type);
    }
  else
    {
      type = dang_builder_get_var_type (builder, var_id);
      dang_assert (type != NULL);
      dang_builder_add_local (builder, name, var_id, type);
    }
  if (flags->permit_uninitialized)
    {
      dang_compile_result_init_stack (result, type, var_id, FALSE,
                                      flags->must_be_lvalue, flags->must_be_rvalue);
      return;
    }
  
  dang_insn_init (&insn, DANG_INSN_TYPE_INIT);
  insn.init.var = var_id;
  dang_builder_note_var_create (builder, var_id);
  dang_builder_add_insn (builder, &insn);

  dang_compile_result_init_stack (result, type, var_id, TRUE,
                                  flags->must_be_lvalue, flags->must_be_rvalue);
}

DANG_BUILTIN_METAFUNCTION (var_decl);
/* === mf-while.c === */

#define syntax_check__while "AA"
#define annotate__while dang_mf_annotate__push_local_scope

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__while)
{
  DangLabelId redo_label, continue_label, break_label;

  DangCompileFlags f = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  DangCompileResult res;
  f.permit_void = 0;
  dang_assert (expr->function.n_args == 2);

  DANG_UNUSED (flags);

  redo_label = dang_builder_start_scoped_label (builder, "$redo");
  continue_label = dang_builder_start_scoped_label (builder, "$continue");
  break_label = dang_builder_start_scoped_label (builder, "$break");
  dang_builder_push_local_scope (builder);
  dang_builder_define_label (builder, continue_label);
  dang_compile (expr->function.args[0], builder, &f, &res);
  dang_builder_add_jump_if_zero (builder, &res, break_label);
  dang_compile_result_clear (&res, builder);
  dang_builder_define_label (builder, redo_label);
  dang_compile (expr->function.args[1], builder, &dang_compile_flags_void, &res);
  dang_compile_result_clear (&res, builder);
  dang_builder_add_jump (builder, continue_label);
  dang_builder_pop_local_scope (builder);
  dang_builder_define_label (builder, break_label);
  dang_compile_result_init_void (result);
}


DANG_BUILTIN_METAFUNCTION(while);


typedef struct {
  unsigned start, count;
} MFInitialCharInfo;

#include "generated-metafunction-table.inc"


DangMetafunction *
dang_metafunction_lookup (const char *name)
{
  if (name[0] != '$')
    return NULL;
  if ('a' <= name[1] && name[1] <= 'z')
    {
      MFInitialCharInfo *ci = mf_initial_char_info + (name[1] - 'a');
      unsigned start = ci->start;
      unsigned count = ci->count;
      while (count > 0)
        {
          unsigned mid = start + count / 2;
          int rv = strcmp (name + 2, mf_table[mid]->name + 2);
          if (rv == 0)
            return mf_table[mid];
          if (rv < 0)
            count /= 2;
          else
            {
              count = start + count - (mid + 1);
              start = mid + 1;
            }
        }
    }
  return NULL;
}

DangMetafunction *
dang_metafunction_lookup_by_expr (DangExpr *expr)
{
  if (expr->type == DANG_EXPR_TYPE_BAREWORD)
    return &dang_bareword_metafunction;
  else if (expr->type == DANG_EXPR_TYPE_VALUE)
    return &dang_value_metafunction;
  else
    return dang_metafunction_lookup (expr->function.name);
}

/* A metafunction for barewords -- they are not even metafunctions,
   so we call them metametafunctions... Values are are also in this category */

#define syntax_check__bareword   NULL

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__bareword)
{
  /* is local? */
  DangVarId var_id;
  DangValueType *type;
  unsigned i;
  if (strcmp (expr->bareword.name, "$void") == 0)
    {
      dang_mf_annotate_statement (annotations, expr);
      return TRUE;
    }
  if (dang_var_table_lookup (var_table, expr->bareword.name, &var_id, &type))
    {
      dang_mf_annotate_value (annotations, expr, type, TRUE, TRUE);
      dang_mf_annotate_local_var_id (annotations, expr, var_id);
      return TRUE;
    }

  /* is function family in default ns? */
  for (i = 0; i < imports->n_imported_namespaces; i++)
    if (imports->imported_namespaces[i].qualifier == NULL)
      {
        DangNamespace *ns = imports->imported_namespaces[i].ns;
        DangNamespaceSymbol *symbol = dang_namespace_lookup (ns, expr->bareword.name);
        if (symbol != NULL)
          {
            /* global, namespace, or function-family */
            dang_mf_annotate_from_namespace_symbol (annotations, expr, ns, symbol);
            return TRUE;
          }
      }


  /* is namespace? */
  for (i = 0; i < imports->n_imported_namespaces; i++)
    if (imports->imported_namespaces[i].qualifier != NULL
         && strcmp (imports->imported_namespaces[i].qualifier, expr->bareword.name) == 0)
      {
        dang_mf_annotate_ns (annotations, expr, imports->imported_namespaces[i].ns);
        return TRUE;
      }

  dang_set_error (error, "symbol '%s' not found ("DANG_CP_FORMAT")",
                  expr->bareword.name, DANG_CP_EXPR_ARGS (expr));
  return FALSE;
}

static DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__bareword)
{
  /* lookup in locals or globals */
  DangVarId id;
  unsigned n_names_used;
  DangNamespace *ns;
  unsigned offset;
  DangValueType *type;
  DangCompileFlags my_flags = DANG_COMPILE_FLAGS_RVALUE_PERMISSIVE;
  dang_boolean is_constant;

  my_flags.must_be_lvalue = flags->must_be_lvalue;
  my_flags.must_be_rvalue = flags->must_be_rvalue;
  my_flags.permit_untyped = 0;
  my_flags.permit_void = 0;
  if (dang_builder_lookup_local (builder, expr->bareword.name, FALSE, &id))
    {
      DangFunctionParamDir dir;
      if (dang_builder_is_param (builder, id, &dir))
        {
          if (dir == DANG_FUNCTION_PARAM_IN
           && flags->must_be_lvalue)
            {
              dang_compile_result_set_error (result, &expr->any.code_position,
                                             "cannot assign to input parameter '%s'", expr->bareword.name);
              return;
            }
          else if (dir == DANG_FUNCTION_PARAM_OUT
           && flags->must_be_rvalue)
            {
              dang_compile_result_set_error (result, &expr->any.code_position,
                                             "cannot read from output parameter '%s'", expr->bareword.name);
              return;
            }
        }
            
      type = dang_builder_get_var_type (builder, id);
      dang_compile_result_init_stack (result, type, id, TRUE,
                                      flags->must_be_lvalue,
                                      flags->must_be_rvalue);
      if (!dang_compile_result_lock (result, builder, &expr->any.code_position,
                                     flags->must_be_lvalue, id,
                                     0, NULL))
        {
          return;
        }
    }
  else if (dang_imports_lookup_global (builder->imports,
                                       1, &expr->bareword.name,
                                       &ns, &offset, &type, &is_constant,
                                       &n_names_used))
    {
      if (is_constant && flags->must_be_lvalue)
        {
          dang_compile_result_set_error (result, &expr->any.code_position,
                                         "constant global %s not assignable",
                                         expr->bareword.name);
          return;
        }
      dang_compile_result_init_global (result, type,
                                       ns, offset,
                                       flags->must_be_lvalue,
                                       flags->must_be_rvalue);
    }
  else if ((ns=dang_imports_lookup_namespace (builder->imports, expr->bareword.name)) != NULL)
    {
      /* Return a literal namespace */
      dang_compile_result_init_literal (result, dang_value_type_namespace (), &ns);
    }
  else
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "could not find symbol %s",
                                     expr->bareword.name);
    }
}


DangMetafunction dang_bareword_metafunction = {
    "_bareword",
    syntax_check__bareword,
    annotate__bareword,
    compile__bareword
};

/* === VALUE === */
#define syntax_check__value   NULL

static DANG_METAFUNCTION_ANNOTATE_FUNC_DECLARE(annotate__value)
{
  DANG_UNUSED (imports);
  DANG_UNUSED (var_table);
  DANG_UNUSED (error);
  if (expr->value.type == dang_value_type_type ())
    {
      dang_mf_annotate_type (annotations, expr, * (DangValueType**) expr->value.value);
    }
  else
    {
      dang_mf_annotate_value (annotations, expr, expr->value.type, FALSE, TRUE);
    }
  return TRUE;
}

static 
DANG_METAFUNCTION_COMPILE_FUNC_DECLARE(compile__value)
{
  if (flags->must_be_lvalue)
    {
      dang_compile_result_set_error (result, &expr->any.code_position,
                                     "literal value cannot be rvalue");
      return;
    }
  /* compile as an assign from literal */
  if (flags->permit_literal)
    {
      dang_compile_result_init_literal (result, expr->value.type, expr->value.value);
    }
  else if (flags->prefer_void)
    {
      result->type = DANG_COMPILE_RESULT_VOID;
      result->any.return_type = NULL;
    }
  else
    {
      DangCompileResult lres;
      DangCompileResult rres;
      DangVarId var = dang_builder_add_tmp (builder, expr->value.type);
      dang_compile_result_init_stack (&lres, expr->value.type, var,
                                      FALSE, TRUE, FALSE);
      dang_compile_result_init_literal_take (&rres, expr->value.type, expr->value.value);
      dang_builder_add_assign (builder, &lres, &rres);
      dang_compile_result_init_stack (result, expr->value.type, var,
                                      TRUE, FALSE, TRUE);
      rres.literal.value = NULL;
      dang_compile_result_clear (&lres, builder);
      dang_compile_result_clear (&rres, builder);
    }
}

DangMetafunction dang_value_metafunction = {
    "_value",
    syntax_check__value,
    annotate__value,
    compile__value
};
