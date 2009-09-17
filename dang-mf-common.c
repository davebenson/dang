#include "dang-mf-common.h"
#include "magic.h"

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
dang_mf_annotate_from_namespace_symbol (DangAnnotations     *annotations,
                                      DangExpr            *expr,
                                      DangNamespaceSymbol *symbol)
{
  switch (symbol->type)
    {
    case DANG_NAMESPACE_SYMBOL_FUNCTIONS:
      dang_mf_annotate_function_family (annotations, expr, symbol->info.functions);
      break;
    case DANG_NAMESPACE_SYMBOL_GLOBAL:
      dang_mf_annotate_value (annotations, expr, symbol->info.global.type, TRUE, TRUE);
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
                       DangArray *var_ids_used)
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
          dang_array_append (var_ids_used, 1, &var_id);
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
  DangArray var_ids = DANG_ARRAY_STATIC_INIT (DangVarId);
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

