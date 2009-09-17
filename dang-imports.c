#include <string.h>
#include "dang.h"

DangImports         *dang_imports_new    (unsigned n_imports,
                                          DangImportedNamespace *imports,
                                          DangNamespace *default_definition_namespace)
{
  DangImports *rv = dang_new (DangImports, 1);
  DangImportedNamespace *ins;
  unsigned i, j;
  rv->ref_count = 1;
  rv->n_imported_namespaces = n_imports;
  rv->imported_namespaces = dang_new (DangImportedNamespace, n_imports);
  rv->required_module_directive = NULL;
  rv->permit_module_directive = TRUE;
  ins = rv->imported_namespaces;
  for (i = 0; i < n_imports; i++)
    {
      ins[i].n_names = imports[i].n_names;
      ins[i].names = dang_new (char *, imports[i].n_names);
      for (j = 0; j < ins[i].n_names; j++)
        ins[i].names[j] = dang_strdup (imports[i].names[j]);
      ins[i].reverse = imports[i].reverse;
      ins[i].qualifier = dang_strdup (imports[i].qualifier);
      ins[i].ns = dang_namespace_ref (imports[i].ns);
    }
  rv->default_definition_namespace = default_definition_namespace
                                        ? dang_namespace_ref (default_definition_namespace)
                                        : NULL;
  return rv;
}
DangImports *
dang_imports_ref    (DangImports *imports)
{
  ++(imports->ref_count);
  return imports;
}
void                 dang_imports_unref  (DangImports *imports)
{
  if (--(imports->ref_count) == 0)
    {
      unsigned i, j;
      for (i = 0; i < imports->n_imported_namespaces; i++)
        {
          for (j = 0; j < imports->imported_namespaces[i].n_names; j++)
            dang_free (imports->imported_namespaces[i].names[j]);
          dang_free (imports->imported_namespaces[i].names);
          dang_free (imports->imported_namespaces[i].qualifier);
          dang_namespace_unref (imports->imported_namespaces[i].ns);
        }
      dang_free (imports->imported_namespaces);
      if (imports->default_definition_namespace)
        dang_namespace_unref (imports->default_definition_namespace);
      dang_free (imports);
    }
}

/* globals */
DangNamespaceSymbol *
dang_imports_lookup (DangImports *imports,
                     unsigned    n_names,
                     char      **names,
                     unsigned   *names_used_out)
{
  unsigned i, n_used;
  for (i = 0; i < imports->n_imported_namespaces; i++)
    {
      DangNamespace *ns;
      DangNamespaceSymbol *symbol = NULL;
      if (imports->imported_namespaces[i].qualifier)
        {
          if (n_names == 1)
            continue;
          if (strcmp (imports->imported_namespaces[i].qualifier, names[0]) != 0)
            continue;
          n_used = 1;
        }
      else
        {
          n_used = 0;
        }
      ns = imports->imported_namespaces[i].ns;
      while (n_used < n_names)
        {
          symbol = dang_namespace_lookup (ns, names[n_used]);
          if (symbol == NULL)
            break;
          if (symbol->type != DANG_NAMESPACE_SYMBOL_NAMESPACE)
            {
              *names_used_out = n_used + 1;
              return symbol;
            }
          ns = symbol->info.ns;
          symbol = NULL;
          n_used++;
        }
    }
  return NULL;
}

DangFunction *
dang_imports_lookup_function (DangImports *imports,
                              unsigned    n_names,
                              char      **names,
                              DangMatchQuery *query,
                              DangError  **error)
{
  unsigned n_names_used;
  DangNamespaceSymbol *symbol = dang_imports_lookup (imports, n_names, names, &n_names_used);
  if (symbol == NULL)
    {
      char *dotted_name = dang_util_join_with_dot (n_names, names);
      dang_set_error (error, "no symbol matching %s", dotted_name);
      dang_free (dotted_name);
      return NULL;
    }
  if (n_names_used < n_names)
    {
      char *dotted_name = dang_util_join_with_dot (n_names_used, names);
      dang_set_error (error, "no symbol %s in namespace %s", names[n_names_used], dotted_name);
      dang_free (dotted_name);
      return NULL;
    }
  if (symbol->type != DANG_NAMESPACE_SYMBOL_FUNCTIONS)
    {
      char *dotted_name = dang_util_join_with_dot (n_names_used, names);
      dang_set_error (error, "non-function found for symbol %s", dotted_name);
      dang_free (dotted_name);
      return NULL;
    }

  DangFunction *function;
  function = dang_function_family_try (symbol->info.functions, query, error);
  if (function != NULL)
    return function;

  return NULL;
}

DangNamespace *
dang_imports_lookup_definition_ns (DangImports *imports,
                                   unsigned    n_names,
                                   char       **names)
{
  unsigned i;
  DangNamespaceSymbol *symbol = NULL;
  if (n_names == 0 && imports->default_definition_namespace != NULL)
    return imports->default_definition_namespace;
  for (i = 0; i < imports->n_imported_namespaces; i++)
    {
      unsigned n_used;
      DangNamespace *ns;
      if (imports->imported_namespaces[i].qualifier)
        {
          if (n_names == 1)
            continue;
          if (strcmp (imports->imported_namespaces[i].qualifier, names[0]) != 0)
            continue;
          n_used = 1;
        }
      else
        {
          n_used = 0;
        }
      ns = imports->imported_namespaces[i].ns;
      while (n_used < n_names)
        {
          symbol = dang_namespace_lookup (ns, names[n_used]);
          if (symbol == NULL)
            break;
          if (symbol->type != DANG_NAMESPACE_SYMBOL_NAMESPACE)
            break;
          ns = symbol->info.ns;
          symbol = NULL;
          n_used++;
        }
      if (n_used == n_names)
        return ns;
    }
  return NULL;
}

dang_boolean
dang_imports_lookup_global  (DangImports *imports,
                             unsigned     n_names,
                             char       **names,
                             DangNamespace **ns_out,
                             unsigned    *ns_offset_out,
                             DangValueType **type_out,
                             unsigned      *n_names_used_out)
{
  unsigned i;
  for (i = 0; i < imports->n_imported_namespaces; i++)
    {
      unsigned n_used;
      DangNamespace *ns;
      if (imports->imported_namespaces[i].qualifier)
        {
          if (n_names == 1)
            continue;
          if (strcmp (imports->imported_namespaces[i].qualifier, names[0]) != 0)
            continue;
          n_used = 1;
        }
      else
        {
          n_used = 0;
        }
      ns = imports->imported_namespaces[i].ns;
      while (n_used < n_names)
        {
          DangNamespaceSymbol *symbol;
          symbol = dang_namespace_lookup (ns, names[n_used]);
          if (symbol == NULL)
            break;
          if (symbol->type == DANG_NAMESPACE_SYMBOL_GLOBAL)
            {
              *ns_out = ns;
              *ns_offset_out = symbol->info.global.offset;
              *type_out = symbol->info.global.type;
              *n_names_used_out = n_used + 1;
              return TRUE;
            }
          else if (symbol->type != DANG_NAMESPACE_SYMBOL_NAMESPACE)
            break;
          ns = symbol->info.ns;
          symbol = NULL;
          n_used++;
        }
    }
  return FALSE;
}

DangValueType *
dang_imports_lookup_type    (DangImports *imports,
                             unsigned     n_names,
                             char       **names)
{
  unsigned i;
  for (i = 0; i < imports->n_imported_namespaces; i++)
    {
      unsigned n_used;
      DangNamespace *ns;
      if (imports->imported_namespaces[i].qualifier)
        {
          if (n_names == 1)
            continue;
          if (strcmp (imports->imported_namespaces[i].qualifier, names[0]) != 0)
            continue;
          n_used = 1;
        }
      else
        {
          n_used = 0;
        }
      ns = imports->imported_namespaces[i].ns;
      while (n_used < n_names)
        {
          DangNamespaceSymbol *symbol;
          symbol = dang_namespace_lookup (ns, names[n_used]);
          if (symbol == NULL)
            break;
          if (symbol->type == DANG_NAMESPACE_SYMBOL_TYPE)
            {
              return symbol->info.type.type;
            }
          else if (symbol->type != DANG_NAMESPACE_SYMBOL_NAMESPACE)
            break;
          ns = symbol->info.ns;
          symbol = NULL;
          n_used++;
        }
    }
  return NULL;
}

DangNamespace *
dang_imports_lookup_namespace (DangImports *imports,
                               const char  *name)
{
  unsigned i;
  for (i = 0; i < imports->n_imported_namespaces; i++)
    if (imports->imported_namespaces[i].qualifier != NULL
     && strcmp (imports->imported_namespaces[i].qualifier, name) != 0)
      {
        return imports->imported_namespaces[i].ns;
      }
    else if (imports->imported_namespaces[i].qualifier == NULL)
      {
        DangNamespaceSymbol *symbol = dang_namespace_lookup (imports->imported_namespaces[i].ns, name);
        if (symbol && symbol->type == DANG_NAMESPACE_SYMBOL_NAMESPACE)
          return symbol->info.ns;
      }
  return NULL;
}
