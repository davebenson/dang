
typedef struct _DangImportedNamespace DangImportedNamespace;
struct _DangImportedNamespace
{
  /* The set of symbols from 'ns' which are imported.
   * if 'reverse', then these are the symbols which are NOT imported.
   */
  unsigned n_names;
  char **names;
  dang_boolean reverse;

  char *qualifier;              /* or NULL for no qualifier */

  DangNamespace *ns;
};

struct _DangImports
{
  unsigned n_imported_namespaces;
  unsigned ref_count;
  DangImportedNamespace *imported_namespaces;
  DangNamespace *default_definition_namespace;
  char **required_module_directive;
  dang_boolean permit_module_directive;
};

DangImports         *dang_imports_new    (unsigned n_imports,
                                          DangImportedNamespace *imports,
                                          DangNamespace *default_definition_namespace);
DangImports         *dang_imports_ref    (DangImports *imports);
void                 dang_imports_unref  (DangImports *imports);

/* globals */
DangNamespaceSymbol *dang_imports_lookup (DangImports *imports,
                                          unsigned    n_names,
                                          char      **names,
                                          unsigned   *named_used_out);
DangFunction        *dang_imports_lookup_function
                                         (DangImports *imports,
                                          unsigned    n_names,
                                          char      **names,
                                          DangMatchQuery *query,
                                          DangError  **error);
DangNamespace       *dang_imports_lookup_namespace (DangImports *imports,
                                                    const char *name);

dang_boolean         dang_imports_lookup_global
                                          (DangImports *imports,
                                           unsigned     n_names,
                                           char       **names,
                                           DangNamespace **ns_out,
                                           unsigned    *ns_offset_out,
                                           DangValueType **type_out,
                                           dang_boolean  *is_constant_out,
                                           unsigned      *n_names_used_out);

DangValueType *     dang_imports_lookup_type
                                          (DangImports *imports,
                                           unsigned     n_names,
                                           char       **names);
