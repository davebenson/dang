#include <string.h>
#include "dang.h"

#define DEBUG_METAFUNCTION_INVOCATION 0

/**
 * dang_compile:
 * @expr:
 * @builder:
 * @flags:
 * @result:
 */
void
dang_compile           (DangExpr            *expr,
                        DangBuilder *builder,
                        DangCompileFlags    *flags,
                        DangCompileResult   *result)
{
  DangExprTag *tag;
  DangMetafunction *mf;
  tag = dang_expr_get_annotation (builder->annotations,
                                  (DangExpr*)expr,
                                  DANG_EXPR_ANNOTATION_TAG);

  if (expr->any.code_position.filename != NULL)
    dang_builder_set_pos (builder, &expr->any.code_position);
  if (tag != NULL)
    {
      if (tag->tag_type == DANG_EXPR_TAG_FUNCTION_FAMILY)
        {
          DangFunction *function = tag->info.ff.function;
          if (function == NULL)
            {
              dang_compile_result_set_error (result, &expr->any.code_position,
                                             "unresolved function-family");
              return;
            }
          if (flags->must_be_lvalue)
            {
              dang_compile_result_set_error (result, &expr->any.code_position,
                                             "function name not an lvalue");
              return;
            }

          if (function->type == DANG_FUNCTION_TYPE_STUB
           && function->stub.cc == NULL)
            dang_compile_context_register (builder->function->stub.cc, function);

          dang_compile_result_init_literal (result,
                                            dang_value_type_function (function->base.sig),
                                            &function);
          dang_compile_obey_flags (builder, flags, result);
          return;
        }
    }
  else
    {
      dang_boolean warn = TRUE;
      if (expr->type == DANG_EXPR_TYPE_FUNCTION
          && expr->function.name[0] == '$')
        warn = FALSE;
      if (warn)
        dang_warning ("un-tagged expr %s", dang_expr_type_name (expr->type));
    }
  mf = dang_metafunction_lookup_by_expr (expr);
  dang_assert (mf != NULL);
  dang_assert (mf->compile != NULL);
  mf->compile (expr, builder, flags, result);

  if (!flags->permit_void
      && result->type == DANG_COMPILE_RESULT_VOID)
    dang_compile_result_set_error (result, &expr->any.code_position,
                                   "void value not allowed here");

}

#ifdef DANG_DEBUG
void _dang_compile__register_debug ()
{
}
#endif
