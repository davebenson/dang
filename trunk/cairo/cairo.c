#include "../dang.h"
#include "../config.h"

static DangValueType *dang_cairo_context_type = NULL;
static DangValueType *dang_cairo_surface_type = NULL;
static DangValueType *dang_cairo_pattern_type = NULL;

void _dang_cairo_init (void)
{
  if (dang_cairo_context_type != NULL)
    return;
  dang_cairo_context_type = dang_new0 (DangValueType, 1);
  dang_cairo_context_type->sizeof_instance = sizeof (void*);
  dang_cairo_context_type->alignof_instance = DANG_ALIGNOF_POINTER;
  dang_cairo_context_type->init_assign = init_assign__cairo_context;
  dang_cairo_context_type->assign = assign__cairo_context;
  dang_cairo_context_type->destruct = destruct__cairo_context;
  dang_cairo_context_type->full_name = "Cairo.Context";
  dang_cairo_surface_type = dang_new0 (DangValueType, 1);
  dang_cairo_surface_type->sizeof_instance = sizeof (void*);
  dang_cairo_surface_type->alignof_instance = DANG_ALIGNOF_POINTER;
  dang_cairo_surface_type->init_assign = init_assign__cairo_surface;
  dang_cairo_surface_type->assign = assign__cairo_surface;
  dang_cairo_surface_type->destruct = destruct__cairo_surface;
  dang_cairo_surface_type->full_name = "Cairo.Surface";
  dang_cairo_pattern_type = dang_new0 (DangValueType, 1);
  dang_cairo_pattern_type->sizeof_instance = sizeof (void*);
  dang_cairo_pattern_type->alignof_instance = DANG_ALIGNOF_POINTER;
  dang_cairo_pattern_type->init_assign = init_assign__cairo_pattern;
  dang_cairo_pattern_type->assign = assign__cairo_pattern;
  dang_cairo_pattern_type->destruct = destruct__cairo_pattern;
  dang_cairo_pattern_type->full_name = "Cairo.Pattern";

  dang_cairo_operator_type = dang_new0 (DangValueType, 1);
  dang_cairo_operator_type->sizeof_instance = sizeof (cairo_operator_t);
  dang_cairo_operator_type->alignof_instance = sizeof (cairo_operator_t);
  dang_cairo_operator_type->to_string = to_string__cairo_operator;

  fparams[0].dir = DANG_FUNCTION_PARAM_IN;
  fparams[0].name = "surface";
  fparams[0].type = dang_cairo_surface_type;
  sig = dang_signature_new (dang_cairo_context_type, 1, fparams);
  f = dang_function_new_simple_c (sig, simple_c__cairo_context_create, NULL, NULL);
  dang_value_type_add_ctor (dang_cairo_context_type, NULL, f);
  dang_function_unref (f);
  dang_signature_unref (sig);

  fparams[0].dir = DANG_FUNCTION_PARAM_IN;
  fparams[0].name = "this";
  fparams[0].type = dang_cairo_context_type;
  sig = dang_signature_new (NULL, 1, fparams);
    f = dang_function_new_simple_c (sig, simple_c__cairo_save, NULL, NULL);
    dang_value_type_add_constant_method (dang_cairo_context_type,
                                         "save", 
                                         DANG_METHOD_PUBLIC|DANG_METHOD_FINAL,
                                         f);
    dang_function_unref (f);
    f = dang_function_new_simple_c (sig, simple_c__cairo_restore, NULL, NULL);
    dang_value_type_add_constant_method (dang_cairo_context_type,
                                         "restore", 
                                         DANG_METHOD_PUBLIC|DANG_METHOD_FINAL,
                                         f);
    dang_function_unref (f);
    f = dang_function_new_simple_c (sig, simple_c__cairo_push_group, NULL, NULL);
    dang_value_type_add_constant_method (dang_cairo_context_type,
                                         "push_group", 
                                         DANG_METHOD_PUBLIC|DANG_METHOD_FINAL,
                                         f);
    dang_function_unref (f);
    f = dang_function_new_simple_c (sig, simple_c__cairo_pop_group, NULL, NULL);
    dang_value_type_add_constant_method (dang_cairo_context_type,
                                         "pop_group", 
                                         DANG_METHOD_PUBLIC|DANG_METHOD_FINAL,
                                         f);
    dang_function_unref (f);
    f = dang_function_new_simple_c (sig, simple_c__cairo_pop_group_to_source, NULL, NULL);
    dang_value_type_add_constant_method (dang_cairo_context_type,
                                         "pop_group_to_source", 
                                         DANG_METHOD_PUBLIC|DANG_METHOD_FINAL,
                                         f);
    dang_function_unref (f);
  dang_signature_unref (sig);

  dang_value_type_add_setget_member (dang_cairo_context_type,
                                     "operator",
                                     DANG_MEMBER_PUBLIC_WRITABLE|
                                     DANG_MEMBER_PUBLIC_READABLE,
                                     dang_cairo_operator_type,
                                     member_set__context_operator,
                                     member_get__context_operator,
                                     NULL, NULL);
  dang_value_type_add_setget_member (dang_cairo_context_type,
                                     "source",
                                     DANG_MEMBER_PUBLIC_WRITABLE|
                                     DANG_MEMBER_PUBLIC_READABLE,
                                     dang_cairo_pattern_type,
                                     member_set__context_source,
                                     member_get__context_source,
                                     NULL, NULL);
  dang_value_type_add_setget_member (dang_cairo_context_type,
                                     "tolerance",
                                     DANG_MEMBER_PUBLIC_WRITABLE|
                                     DANG_MEMBER_PUBLIC_READABLE,
                                     dang_value_type_double (),
                                     member_set__context_tolerance,
                                     member_get__context_tolerance,
                                     NULL, NULL);
  dang_value_type_add_setget_member (dang_cairo_context_type,
                                     "antialias",
                                     DANG_MEMBER_PUBLIC_WRITABLE|
                                     DANG_MEMBER_PUBLIC_READABLE,
                                     dang_cairo_antialias_type
                                     member_set__context_antialias,
                                     member_get__context_antialias,
                                     NULL, NULL);
  dang_value_type_add_setget_member (dang_cairo_context_type,
                                     "fill_rule",
                                     DANG_MEMBER_PUBLIC_WRITABLE|
                                     DANG_MEMBER_PUBLIC_READABLE,
                                     dang_cairo_fill_rule_type
                                     member_set__context_fill_rule,
                                     member_get__context_fill_rule,
                                     NULL, NULL);
  dang_value_type_add_setget_member (dang_cairo_context_type,
                                     "line_width",
                                     DANG_MEMBER_PUBLIC_WRITABLE|
                                     DANG_MEMBER_PUBLIC_READABLE,
                                     dang_value_type_double (),
                                     member_set__context_line_width,
                                     member_get__context_line_width,
                                     NULL, NULL);

  /* set source rgb, rgba */
  ...

  /* 
