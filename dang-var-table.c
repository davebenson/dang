#include <string.h>
#include "dang.h"

typedef DangVarTableScope Scope;
typedef DangVarTableVariable Variable;


DangVarTable *dang_var_table_new (dang_boolean has_rv)
{
  DangVarTable *var_table = dang_new (DangVarTable, 1);
  DANG_ARRAY_INIT (&var_table->variables, Variable);
  DANG_ARRAY_INIT (&var_table->scopes, Scope);
  var_table->has_rv = has_rv;
  dang_var_table_push (var_table);
  return var_table;
}

void dang_var_table_push (DangVarTable *var_table)
{
  Scope scope = { DANG_ARRAY_STATIC_INIT(DangVarId) };
  dang_array_append (&var_table->scopes, 1, &scope);
}

void dang_var_table_pop (DangVarTable *var_table)
{
  Scope *to_kill;
  dang_assert (var_table->scopes.len > 0);
  to_kill = (Scope*) var_table->scopes.data + var_table->scopes.len - 1;
  dang_array_clear (&to_kill->var_ids);
  dang_array_set_size (&var_table->scopes, var_table->scopes.len - 1);
}

static DangVarId alloc_local (DangVarTable *var_table,
                                                 const char             *name,
                                                 DangExpr               *decl_expr,
                                                 DangValueType          *type,
                                                 dang_boolean is_param,
                                                 DangFunctionParamDir dir)
{
  Scope *last;
  Variable *vars = var_table->variables.data;
  Variable v;
  DangVarId *var_ids;
  unsigned i;
  DangVarId rv;
  dang_assert (var_table->scopes.len > 0);
  last = (Scope*) var_table->scopes.data + var_table->scopes.len - 1;
  var_ids = last->var_ids.data;
  for (i = 0; i < last->var_ids.len; i++)
    if (strcmp (vars[var_ids[i]].name, name) == 0)
      return DANG_VAR_ID_INVALID;
  v.name = dang_strdup (name);
  v.decl = decl_expr;
  v.type_expr = type ? decl_expr : NULL;
  v.type = type;
  v.is_param = is_param;
  v.param_dir = dir;
  rv = var_table->variables.len;
  dang_array_append (&var_table->variables, 1, &v);
  dang_array_append (&last->var_ids, 1, &rv);
  return rv;
}
DangVarId dang_var_table_alloc_local (DangVarTable *var_table,
                                                 const char             *name,
                                                 DangExpr               *decl_expr,
                                                 DangValueType          *type)
{
  return alloc_local (var_table, name, decl_expr, type, FALSE, 0);
}

dang_boolean dang_var_table_lookup (DangVarTable *var_table,
                                               const char *name,
                                               DangVarId *var_id_out,
                                               DangValueType **type_out)
{
  unsigned n_scopes = var_table->scopes.len;
  Scope *scopes = var_table->scopes.data;
  unsigned s, i;
  Variable *vars = var_table->variables.data;
  for (s = 0; s < n_scopes; s++)
    {
      Scope *sc = scopes + s;
      DangVarId *var_ids = sc->var_ids.data;
      for (i = 0; i < sc->var_ids.len; i++)
        if (strcmp (vars[var_ids[i]].name, name) == 0)
          {
            *var_id_out = var_ids[i];
            *type_out = vars[var_ids[i]].type;
            return TRUE;
          }
    }
  return FALSE;
}
DangValueType * dang_var_table_get_type (DangVarTable *var_table,
                                                  DangVarId id)
{
  dang_assert (id < var_table->variables.len);
  Variable *vars = var_table->variables.data;
  return vars[id].type;
}
const char    * dang_var_table_get_name (DangVarTable *var_table,
                                                  DangVarId id)
{
  dang_assert (id < var_table->variables.len);
  Variable *vars = var_table->variables.data;
  return vars[id].name;
}
void            dang_var_table_set_type (DangVarTable *var_table,
                                                  DangVarId id,
                                                  DangExpr *loc,
                                                  DangValueType *type)
{
  Variable *vars = var_table->variables.data;
  dang_assert (id < var_table->variables.len);
  dang_assert (vars[id].type == NULL);
  vars[id].type_expr = loc;
  vars[id].type = type;
}

void dang_var_table_add_params (DangVarTable *table,
                                DangValueType *rv_type, /* or NULL */
                                unsigned n_params,
                                DangFunctionParam *params)
{
  unsigned i;
  if (table->has_rv)
    {
      if (rv_type == dang_value_type_void ())
        rv_type = NULL;
      alloc_local (table, "return_value", NULL, rv_type, TRUE,
                   DANG_FUNCTION_PARAM_OUT);
    }
  for (i = 0; i < n_params; i++)
    alloc_local (table, params[i].name, NULL, params[i].type, TRUE,
                 params[i].dir);
}

void dang_var_table_free (DangVarTable *var_table)
{
  Scope *scopes = var_table->scopes.data;
  Variable *vars = var_table->variables.data;
  unsigned i;
  for (i = 0; i < var_table->scopes.len; i++)
    dang_array_clear (&scopes[i].var_ids);
  for (i = 0; i < var_table->variables.len; i++)
    dang_free (vars[i].name);
  dang_free (vars);
  dang_free (scopes);
  dang_free (var_table);
}
