
typedef enum
{
  DANG_NAMESPACE_SYMBOL_FUNCTIONS,              /* overloaded */
  DANG_NAMESPACE_SYMBOL_GLOBAL,
  DANG_NAMESPACE_SYMBOL_NAMESPACE,
  DANG_NAMESPACE_SYMBOL_TYPE
} DangNamespaceSymbolType;
const char *dang_namespace_symbol_type_name (DangNamespaceSymbolType);

struct _DangNamespaceSymbol
{
  DangNamespaceSymbolType type;
  union {
    DangFunctionFamily *functions;
    struct {
      DangValueType *type;
      unsigned offset;
    } global;
    struct {
      DangValueType *type;
    } type;
    DangNamespace *ns;
  } info;
};

typedef struct _DangNamespaceName DangNamespaceName;
struct _DangNamespaceName
{
  char *name;
  DangNamespaceSymbol symbol;

  /* rbtree lookups */
  DangNamespaceName *left, *right, *parent;
  dang_boolean is_red;
};

struct _DangNamespace
{
  char *full_name;
  unsigned ref_count;
  DangNamespaceName *by_name;
  uint8_t *global_data;
  unsigned global_data_size;
  unsigned global_data_alloced;
};


DangNamespace *dang_namespace_default (void);
DangNamespace *dang_namespace_new    (const char    *full_name);
DangNamespace *dang_namespace_ref    (DangNamespace *ns);
void           dang_namespace_unref  (DangNamespace *ns);

DangNamespaceSymbol *
               dang_namespace_lookup (DangNamespace *ns,
                                      const char    *name);

DangNamespace *dang_namespace_force  (unsigned       n_names,
                                      char         **names,
                                      DangError    **error);
DangNamespace *dang_namespace_try    (unsigned       n_names,
                                      char         **names,
                                      DangError    **error);
/* Not recommended, use lookup + add_namespace. */
#if 0
/* child namespace will be created if it doesn't exist */
DangNamespace *dang_namespace_get_child (DangNamespace *ns,
                                         const char    *child);

/* returns NULL if child namespace doesn't exist */
DangNamespace *dang_namespace_try_child (DangNamespace *ns,
                                         const char    *child);
#endif

/* standard non-lazy function */
dang_boolean  dang_namespace_add_function (DangNamespace     *ns,
                                           const char        *name,
                                           DangFunction      *function,
                                           DangError        **error);
dang_boolean  dang_namespace_add_function_family
                                          (DangNamespace     *ns,
                                           const char        *name,
                                           DangFunctionFamily *family,
                                           DangError        **error);
dang_boolean  dang_namespace_check_function(DangNamespace    *ns,
                                           const char        *name,
                                           DangSignature     *sig,
                                           DangError        **error);

/* globals */
dang_boolean  dang_namespace_add_global   (DangNamespace *ns,
                                           const char    *name,
                                           DangValueType *type,
                                           const void    *value,
                                           unsigned      *ns_offset_out,
                                           DangError        **error);

dang_boolean  dang_namespace_add_type      (DangNamespace *ns,
                                            const char    *name,
                                            DangValueType *type,
                                           DangError        **error);

/* subnamespace */
dang_boolean  dang_namespace_add_namespace (DangNamespace *ns,
                                            const char    *name,
                                            DangNamespace *child,
                                            DangError        **error);

/* A convenience function for libraries that have to register a lot of functions. */
void dang_namespace_add_simple_c             (DangNamespace *ns,
                                              const char    *name,
                                              DangSignature *sig,
                                              DangSimpleCFunc func,
                                              void           *func_data);
/* params are (dir, name, type)-triples. */
void dang_namespace_add_simple_c_from_params (DangNamespace *ns,
                                              const char    *name,
                                              DangSimpleCFunc func,
                                              DangValueType *rv_type,
                                              unsigned       n_params,
                                              ...);
DangFunction *dang_function_new_simple_c_from_params (DangSimpleCFunc func,
                                             DangValueType *rv_type,
                                             unsigned       n_params,
                                             ...);
