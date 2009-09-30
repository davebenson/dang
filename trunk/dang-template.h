

/* A non-instantiable type, which is unique by name */
typedef struct _DangValueTypeTemplateParam DangValueTypeTemplateParam;
struct _DangValueTypeTemplateParam
{
  DangValueType base_type;
  char *formal_name;

  DangValueTypeTemplateParam *left, *right, *parent;
  dang_boolean is_red;
};

DangValueType *dang_value_type_template_param (const char *name);
dang_boolean dang_value_type_is_template_param (DangValueType *);

//void         dang_templated_type_find_params (DangValueType *type,
//                                              unsigned      *n_params_out,
//                                              DangValueType ***params_out);

/* 'pairs' is an array of template-param/concrete-type pairs. */
void         dang_type_gather_template_params (DangValueType *,
                                               DangUtilArray *tparams_inout);
dang_boolean dang_templated_type_check (DangValueType *templated_type,
                                        DangValueType *match_type,
                                        DangUtilArray     *pairs_out);
DangValueType *dang_templated_type_make_concrete (DangValueType *templated_type,
                                                  DangUtilArray *tt_pairs);

dang_boolean dang_expr_contains_disallowed_template_param (DangExpr *,
                                                unsigned n_allowed,
                                                DangValueType **allowed,
                                                const char **bad_name_out,
                                                DangExpr **bad_expr_out);
dang_boolean dang_value_type_contains_disallowed_template_param (DangValueType *,
                                                unsigned n_allowed,
                                                DangValueType **allowed,
                                                const char **bad_name_out);

DangExpr *dang_templated_expr_substitute_types (DangExpr *orig,
                                          DangUtilArray *pairs);
