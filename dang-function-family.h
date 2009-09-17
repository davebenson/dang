/* A family of functions which differ in signature. */


typedef enum
{
  DANG_FUNCTION_FAMILY_CONTAINER,
  DANG_FUNCTION_FAMILY_VARIADIC_C,
  DANG_FUNCTION_FAMILY_TEMPLATE
} DangFunctionFamilyType;

/* NOTE: this may or may not set 'error' !!! */
typedef DangFunction *(*DangFunctionTrySigFunc) (DangMatchQuery *query,
                                                 void *data,
                                                 DangError **error);
#define DANG_FUNCTION_TRY_SIG_FUNC_DECLARE(func_name) \
        DangFunction *       func_name          (DangMatchQuery *query, \
                                                 void *data, \
                                                 DangError **error)


struct _DangFunctionFamily
{
  DangFunctionFamilyType type;
  unsigned ref_count;
  char *name;
  union
  {
    struct {
      DangArray functions;
      DangArray families;
    } container;
    struct {
      DangFunctionTrySigFunc try_sig;
      void *data;
      DangDestroyNotify destroy;
    } variadic_c;
    struct {
      DangImports *imports;
      DangSignature *sig;                   /* params and rv may be templated */
      //unsigned n_tparams;
      //DangValueType **tparams;
      DangExpr *body_expr;
      DangValueType *method_type;           /* if a method */
      unsigned n_friends;
      DangValueType **friends;
    } templat;          /* 'e' is omitted for c++ compat */
  } info;
};

DangFunctionFamily *dang_function_family_ref (DangFunctionFamily *family);
void                dang_function_family_unref (DangFunctionFamily *family);
DangFunction       *dang_function_family_try (DangFunctionFamily *family,
                                              DangMatchQuery     *match_query,
                                              DangError         **error);

/* --- container --- */
DangFunctionFamily *dang_function_family_new (const char *name);
void                dang_function_family_container_add
                                             (DangFunctionFamily *container,
                                              DangFunctionFamily *subfamily);
void                dang_function_family_container_add_function
                                             (DangFunctionFamily *container,
                                              DangFunction       *function);

dang_boolean        dang_function_family_check_conflict
                                             (DangFunctionFamily *family,
                                              DangSignature      *sig,
                                              DangError         **error);
/* refs function */
DangFunction *dang_function_family_is_single (DangFunctionFamily *);

/* --- variadic function --- */
DangFunctionFamily *dang_function_family_new_variadic_c (const char *name,
                                                         DangFunctionTrySigFunc try_sig,
                                                       void *data,
                                                       DangDestroyNotify destroy);

/* --- template --- */
DangFunctionFamily *dang_function_family_new_template
                                             (const char         *name,
                                              DangImports        *imports,
                                              DangSignature      *sig,  /* must be templated */
                                              DangExpr           *body_expr,
                                              DangError         **error);
DangFunctionFamily *dang_function_family_new_template_method
                                             (const char         *name,
                                              unsigned            n_params,
                                              DangFunctionParam  *params,
                                              DangExpr           *body_expr,
                                              DangValueType *method_type,
                                              unsigned n_friends,
                                              DangValueType **friends);



/* --- concat -- */
DangFunction *dang_function_concat_peek (unsigned n_args);


/* diagnostics */
void dang_function_family_dump (DangFunctionFamily *ff,
                                const char         *name,
                                DangStringBuffer   *str);
