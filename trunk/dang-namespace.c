#include <string.h>
#include "dang.h"
#include "gskrbtreemacros.h"

const char *
dang_namespace_symbol_type_name (DangNamespaceSymbolType type)
{
  switch (type)
    {
    case DANG_NAMESPACE_SYMBOL_FUNCTIONS:
      return "functions";
    case DANG_NAMESPACE_SYMBOL_GLOBAL:
      return "global";
    case DANG_NAMESPACE_SYMBOL_NAMESPACE:
      return "namespace";
    case DANG_NAMESPACE_SYMBOL_TYPE:
      return "type";
    default:
      return "*bad type*";
    }
}

#define GET_IS_RED(n) n->is_red
#define SET_IS_RED(n,v) n->is_red=v
#define BY_NAME_TREE_COMPARE(a,b,rv) rv = strcmp(a->name, b->name)
#define GET_BY_NAME_TREE(ns) \
  (ns)->by_name, DangNamespaceName *, GET_IS_RED, SET_IS_RED, parent, left, right, \
  BY_NAME_TREE_COMPARE
#define STRING__KEY_COMPARE(a,b,rv) rv = strcmp (a, b->name)

DangNamespace *dang_namespace_new    (const char    *full_name)
{
  DangNamespace *rv = dang_new (DangNamespace, 1);
  rv->full_name = dang_strdup (full_name);
  rv->ref_count = 1;
  rv->by_name = NULL;
  rv->global_data = NULL;
  rv->global_data_size = 0;
  rv->global_data_alloced = 0;
  return rv;
}
DangNamespace *dang_namespace_ref    (DangNamespace *ns)
{
  //dang_warning ("dang_namespace_ref: %s: %u => %u", ns->full_name, ns->ref_count, ns->ref_count+1);
  ++(ns->ref_count);
  return ns;
}

static void
delete_by_name_tree_recursive (DangNamespace *ns,
                               DangNamespaceName *name)
{
  DangNamespaceName *left = name->left;
  DangNamespaceName *right = name->right;
  dang_free (name->name);

  switch (name->symbol.type)
    {
    case DANG_NAMESPACE_SYMBOL_FUNCTIONS:
      dang_function_family_unref (name->symbol.info.functions);
      break;
    case DANG_NAMESPACE_SYMBOL_GLOBAL:
      {
        DangValueType *type = name->symbol.info.global.type;
        unsigned offset = name->symbol.info.global.offset;
        if (type->destruct)
          type->destruct (type, ns->global_data + offset);
      }
      break;
    case DANG_NAMESPACE_SYMBOL_NAMESPACE:
      dang_namespace_unref (name->symbol.info.ns);
      break;
    case DANG_NAMESPACE_SYMBOL_TYPE:
      break;
    }
  dang_free (name);

  if (left)
    delete_by_name_tree_recursive (ns, left);
  if (right)
    delete_by_name_tree_recursive (ns, right);
}
void
dang_namespace_unref  (DangNamespace *ns)
{
  //dang_warning ("dang_namespace_unref: %s: %u => %u", ns->full_name, ns->ref_count, ns->ref_count-1);
  if (--(ns->ref_count) == 0)
    {
      if (ns->by_name)
        delete_by_name_tree_recursive (ns, ns->by_name);
      dang_free (ns->global_data);
      dang_free (ns->full_name);
      dang_free (ns);
    }
}

DangNamespaceSymbol *
dang_namespace_lookup (DangNamespace *ns,
                       const char    *name)
{
  DangNamespaceName *out = NULL;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_BY_NAME_TREE (ns), name, STRING__KEY_COMPARE, out);
  return out ? &out->symbol : NULL;
}

DangNamespace *dang_namespace_force  (unsigned       n_names,
                                      char         **names,
                                      DangError    **error)
{
  DangNamespace *at = dang_namespace_default ();
  unsigned i;
  for (i = 0; i < n_names; i++)
    {
      DangNamespaceSymbol *sym = dang_namespace_lookup (at, names[i]);
      if (sym == NULL)
        {
          char *n = dang_util_join_with_dot (i+1, names);
          DangNamespace *new = dang_namespace_new (n);
          dang_free (n);
          if (!dang_namespace_add_namespace (at, names[i], new, error))
            return NULL;                /* can't happen */
          dang_namespace_unref (new);
          at = new;                     /* safe, because dang_namespace_add_namespace() refs new */
        }
      else if (sym->type != DANG_NAMESPACE_SYMBOL_NAMESPACE)
        {
          dang_set_error (error, "symbol %s already defined as a %s in %s",
                          names[i], dang_namespace_symbol_type_name (sym->type), at->full_name);
          return NULL;
        }
      else
        at = sym->info.ns;
    }
  return at;
}

DangNamespace *dang_namespace_try    (unsigned       n_names,
                                      char         **names,
                                      DangError    **error)
{
  DangNamespace *at = dang_namespace_default ();
  unsigned i;
  for (i = 0; i < n_names; i++)
    {
      DangNamespaceSymbol *sym = dang_namespace_lookup (at, names[i]);
      if (sym == NULL)
        {
          char *n = dang_util_join_with_dot (i+1, names);
          dang_set_error (error, "namespace %s not found", n);
          dang_free (n);
          return NULL;
        }
      else if (sym->type != DANG_NAMESPACE_SYMBOL_NAMESPACE)
        {
          dang_set_error (error, "symbol %s already defined as a %s in %s",
                          names[i], dang_namespace_symbol_type_name (sym->type), at->full_name);
          return NULL;
        }
      else
        at = sym->info.ns;
    }
  return at;
}

static DangFunctionFamily *
force_namespace_function (DangNamespace *ns,
                             const char        *name,
                             DangError        **error)
{
  DangNamespaceName *out = NULL;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_BY_NAME_TREE (ns), name, STRING__KEY_COMPARE, out);
  if (out != NULL)
    {
      if (out->symbol.type != DANG_NAMESPACE_SYMBOL_FUNCTIONS)
        {
          dang_set_error (error, "symbol %s already defined as a %s in %s",
                          name, dang_namespace_symbol_type_name (out->symbol.type),
                          ns->full_name);
          return NULL;
        }
    }
  else
    {
      DangNamespaceName *unused;
      out = dang_new (DangNamespaceName, 1);
      out->name = dang_strdup (name);
      out->symbol.type = DANG_NAMESPACE_SYMBOL_FUNCTIONS;
      out->symbol.info.functions = dang_function_family_new (name);
      GSK_RBTREE_INSERT (GET_BY_NAME_TREE (ns), out, unused);
    }
  return out->symbol.info.functions;
}

dang_boolean
dang_namespace_add_function (DangNamespace     *ns,
                             const char        *name,
                             DangFunction      *function,
                             DangError        **error)
{
  DangFunctionFamily *container;
  //dang_warning ("adding %s to %s", name, ns->full_name);
#ifdef DANG_DEBUG
  if (function->type == DANG_FUNCTION_TYPE_SIMPLE_C)
    dang_debug_register_simple_c (function->simple_c.func, ns, name);
#endif
  container = force_namespace_function (ns, name, error);
  if (!container)
    return FALSE;
  dang_function_family_container_add_function (container, function);
  return TRUE;
}
dang_boolean
dang_namespace_add_function_family (DangNamespace     *ns,
                                    const char        *name,
                                    DangFunctionFamily*family,
                                    DangError        **error)
{
  DangFunctionFamily *container;
  container = force_namespace_function (ns, name, error);
  if (!container)
    return FALSE;
  dang_function_family_container_add (container, family);
  return TRUE;
}

dang_boolean
dang_namespace_check_function (DangNamespace     *ns,
                               const char        *name,
                               DangSignature     *sig,
                               DangError        **error)
{
  DangNamespaceSymbol *symbol = dang_namespace_lookup (ns, name);
  if (symbol == NULL)
    return TRUE;
  if (symbol->type != DANG_NAMESPACE_SYMBOL_FUNCTIONS)
    {
      dang_set_error (error, "symbol %s already defined as a %s in %s",
                      name, dang_namespace_symbol_type_name (symbol->type),
                      ns->full_name);
      return FALSE;
    }
  return dang_function_family_check_conflict (symbol->info.functions,
                                              sig, error);
    
}

static DangNamespaceName *
define_unique (DangNamespace     *ns,
               const char        *name,
               DangNamespaceSymbolType type,
               DangError        **error)
{
  DangNamespaceName *out = NULL;
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_BY_NAME_TREE (ns), name, STRING__KEY_COMPARE, out);
  if (out != NULL)
    {
      dang_set_error (error, "symbol %s already defined as a %s in %s",
                      name, dang_namespace_symbol_type_name (out->symbol.type),
                      ns->full_name);
      return NULL;
    }
  else
    {
      DangNamespaceName *unused;
      out = dang_new (DangNamespaceName, 1);
      out->name = dang_strdup (name);
      out->symbol.type = type;
      GSK_RBTREE_INSERT (GET_BY_NAME_TREE (ns), out, unused);
      return out;
    }
}

static dang_boolean
namespace_add_global_internal(DangNamespace *ns,
                             const char    *name,
                             DangValueType *type,
                             dang_boolean   is_constant,
                             const void    *value,
                             unsigned      *offset_out,
                             DangError    **error)
{
  DangNamespaceName *n = define_unique (ns, name, DANG_NAMESPACE_SYMBOL_GLOBAL, error);
  unsigned offset;
  if (n == NULL)
    return FALSE;

  offset = ns->global_data_size;
  if ((offset & (type->alignof_instance - 1)) != 0)
    offset += type->alignof_instance - (offset & (type->alignof_instance - 1));
  /* Compute the new size of the global space */
  ns->global_data_size = offset + type->sizeof_instance;

  /* Resize our allocation if it's too small */
  if (ns->global_data_size > ns->global_data_alloced)
    {
      unsigned new_alloced = ns->global_data_alloced ? ns->global_data_alloced * 2 : 16;
      while (new_alloced < ns->global_data_size)
        new_alloced *= 2;
      ns->global_data = dang_realloc (ns->global_data, new_alloced);
    }

  /* copy in the global */
  if (value == NULL)
    memset (ns->global_data + offset, 0, type->sizeof_instance);
  else if (type->init_assign)
    type->init_assign (type, ns->global_data + offset, value);
  else
    memcpy (ns->global_data + offset, value, type->sizeof_instance);

  n->symbol.info.global.type = type;
  n->symbol.info.global.offset = offset;
  n->symbol.info.global.is_constant = is_constant;
  if (offset_out)
    *offset_out = offset;
  return TRUE;
}
dang_boolean
dang_namespace_add_global   (DangNamespace *ns,
                             const char    *name,
                             DangValueType *type,
                             const void    *value,
                             unsigned      *offset_out,
                             DangError    **error)
{
  return namespace_add_global_internal (ns, name, type, FALSE, value, offset_out, error);
}

dang_boolean
dang_namespace_add_const_global(DangNamespace *ns,
                             const char    *name,
                             DangValueType *type,
                             const void    *value,
                             unsigned      *offset_out,
                             DangError    **error)
{
  return namespace_add_global_internal (ns, name, type, TRUE, value, offset_out, error);
}


dang_boolean
dang_namespace_add_namespace (DangNamespace *ns,
                              const char    *name,
                              DangNamespace *child,
                              DangError    **error)
{
  DangNamespaceName *n = define_unique (ns, name, DANG_NAMESPACE_SYMBOL_NAMESPACE, error);
  if (n == NULL)
    return FALSE;
  n->symbol.info.ns = dang_namespace_ref (child);
  return TRUE;
}


dang_boolean
dang_namespace_add_type  (DangNamespace *ns,
                          const char    *name,
                          DangValueType *type,
                          DangError    **error)
{
  DangNamespaceName *n = define_unique (ns, name, DANG_NAMESPACE_SYMBOL_TYPE, error);
  if (n == NULL)
    return FALSE;
  n->symbol.info.type.type = type;
  return TRUE;
}


DangFunction *
dang_function_new_simple_c_from_params_valist (DangSimpleCFunc func,
                                        DangValueType *rv_type,
                                        unsigned       n_params,
                                        va_list        args)
{
  DangFunctionParam *params = alloca (sizeof (DangFunctionParam) * n_params);
  unsigned i;
  DangSignature *sig;
  DangFunction *function;
  for (i = 0; i < n_params; i++)
    {
      params[i].dir = va_arg (args, DangFunctionParamDir);
      params[i].name = va_arg (args, const char *);
      params[i].type = va_arg (args, DangValueType *);
    }

  sig = dang_signature_new (rv_type, n_params, params);
  function = dang_function_new_simple_c (sig, func, NULL, NULL);
  dang_signature_unref (sig);
  return function;
}
DangFunction *
dang_function_new_simple_c_from_params (DangSimpleCFunc func,
                                        DangValueType *rv_type,
                                        unsigned       n_params,
                                        ...)
{
  va_list args;
  DangFunction *rv;
  va_start (args, n_params);
  rv = dang_function_new_simple_c_from_params_valist (func, rv_type,
                                                      n_params, args);
  va_end (args);
  return rv;
}

void
dang_namespace_add_simple_c_from_params (DangNamespace *ns,
                             const char    *name,
                             DangSimpleCFunc func,
                             DangValueType *rv_type,
                             unsigned       n_params,
                             ...)
{
  DangFunction *function;
  va_list args;
  DangError *error = NULL;
  va_start (args, n_params);
  function = dang_function_new_simple_c_from_params_valist (func, rv_type,
                                                      n_params, args);
  va_end (args);
  if (!dang_namespace_add_function (ns, name, function, &error))
    {
      dang_die ("error adding simple c function '%s' to namespace '%s' (%s)",
                ns->full_name, name, error->message);
    }
  dang_function_unref (function);
}


void dang_namespace_add_simple_c             (DangNamespace *ns,
                                              const char    *name,
                                              DangSignature *sig,
                                              DangSimpleCFunc func,
                                              void           *func_data)
{
  DangFunction *function;
  DangError *error = NULL;
  function = dang_function_new_simple_c (sig, func, func_data, NULL);
  if (!dang_namespace_add_function (ns, name, function, &error))
    {
      dang_die ("error adding simple c function '%s' to namespace '%s' (%s)",
                ns->full_name, name, error->message);
    }
  dang_function_unref (function);
}
