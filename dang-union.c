#include <string.h>
#include "dang.h"
#include "magic.h"

static DangValueTypeUnion *global_union_list;

static void
init_assign__union  (DangValueType *type,
                     void          *dst,
                     const void    *src)
{
  DangValueTypeUnion *u = (DangValueTypeUnion *) type;
  unsigned code = u->read_code (src);
  DangUnionCase *c;
  dang_assert (code < u->n_cases);
  c = u->cases + code;
  if (c->struct_type->init_assign == NULL)
    memcpy (dst, src, c->struct_type->sizeof_instance);
  else
    {
      c->struct_type->init_assign (c->struct_type, dst, src);
      memcpy (dst, src, u->enum_type->sizeof_instance);
    }
}

static void
assign__union       (DangValueType *type,
                     void          *dst,
                     const void    *src)
{
  DangValueTypeUnion *u = (DangValueTypeUnion *) type;
  unsigned old_code = u->read_code (dst);
  unsigned new_code = u->read_code (src);
  DangUnionCase *old_c, *new_c;
  dang_assert (old_code < u->n_cases);
  dang_assert (new_code < u->n_cases);
  if (src == dst)
    return;
  old_c = u->cases + old_code;
  new_c = u->cases + new_code;
  /* Destruct old value */
  if (old_c->struct_type->destruct != NULL)
    old_c->struct_type->destruct (old_c->struct_type, dst);
  if (new_c->struct_type->init_assign == NULL)
    memcpy (dst, src, new_c->struct_type->sizeof_instance);
  else
    {
      new_c->struct_type->init_assign (new_c->struct_type, dst, src);
      memcpy (dst, src, u->enum_type->sizeof_instance);
    }
}

static void
destruct__union     (DangValueType *type,
                     void          *value)
{
  DangValueTypeUnion *u = (DangValueTypeUnion *) type;
  unsigned code = u->read_code (value);
  DangUnionCase *c;
  dang_assert (code < u->n_cases);
  c = u->cases + code;
  if (c->struct_type->destruct != NULL)
    c->struct_type->destruct (c->struct_type, value);
}

static char *
to_string__union (DangValueType *type,
                  const void    *value)
{
  DangValueTypeUnion *u = (DangValueTypeUnion *) type;
  unsigned code = u->read_code (value);
  DangUnionCase *c;
  char *struct_str;
  dang_assert (code < u->n_cases);
  c = u->cases + code;
  struct_str = c->struct_type->to_string (c->struct_type, value);
  
  /* CONSIDER: alter struct_str in some way? */

  return struct_str;
}

static void
dang_compile_check_union_type (DangBuilder *builder,
                               DangCompileResult *container,
                               DangValueTypeUnion *utype,
                               unsigned case_index)
{
  DangCompileResult args[2];
  DangCompileResult func;
  uint32_t expected = case_index;
  args[0] = *container;
  dang_compile_result_init_literal (args + 1, dang_value_type_uint32 (), &expected);
  dang_compile_result_init_literal (&func, dang_value_type_function (utype->check_union_code->base.sig), &utype->check_union_code);
  dang_compile_function_invocation (&func, builder, NULL, 2, args);
  dang_compile_result_clear (args + 1, builder);
  dang_compile_result_clear (&func, builder);
}

/* TODO: OPTIMIZATION: someday we can optimize out the two
   parts of this equation. 
      (1) if the union's type won't be affected during the lifetime
      of the return-value, then, we can make the return-val
      an alias of the input
      (2) if the union's type can be proven correct, we can omit
      the type-check. */

typedef struct _LValueData LValueData;
struct _LValueData
{
  DangCompileResult union_res;
  unsigned case_index;
};

static void
compile_lvalue_callback (DangCompileResult   *result,
                         DangBuilder *builder)
{
  LValueData *lvdata = result->stack.callback_data;
  DangValueTypeUnion *utype = (DangValueTypeUnion*) lvdata->union_res.any.return_type;
  DangValueType *stype = utype->cases[lvdata->case_index].struct_type;
  DangCompileResult casted_cont;

  dang_compile_check_union_type (builder, &lvdata->union_res, utype, lvdata->case_index);

  /* Add an alias to 'container'
     and compile an assignment (using the struct_type). */
  casted_cont = lvdata->union_res;
  casted_cont.any.return_type = stype;
  dang_builder_add_assign (builder, &casted_cont, result);
}

static void
free_lvalue_callback_data (void *data,
                            DangBuilder *builder)
{
  LValueData *lvd = data;
  dang_compile_result_clear (&lvd->union_res, builder);
  dang_free (lvd);
}

static dang_boolean
compile_access__union (DangBuilder   *builder,
                       DangCompileResult     *container,
                       void                  *member_data,
                       DangCompileFlags      *flags,
                       DangCompileResult     *result)
{
  DangValueType *cont_type = container->any.return_type;
  DangValueTypeUnion *utype = (DangValueTypeUnion *) cont_type;
  unsigned case_index = DANG_POINTER_TO_UINT (member_data);
  DangValueType *stype = utype->cases[case_index].struct_type;
  DangCompileResult casted_cont;
  DangVarId ret_var_id;
  dang_assert (dang_value_type_is_union (cont_type));
  dang_compile_check_union_type (builder, container, utype, case_index);

  /* Allocate the return variable. */
  ret_var_id = dang_builder_add_tmp (builder, stype);

  if (flags->must_be_rvalue)
    {
      /* Add an alias to 'container'
         and compile an assignment (using the struct_type). */
      casted_cont = *container;
      casted_cont.any.return_type = stype;

      dang_compile_result_init_stack (result, stype, ret_var_id,
                                      FALSE, TRUE, FALSE);
      dang_builder_add_assign (builder, result, &casted_cont);
      result->stack.was_initialized = TRUE;
      result->any.is_lvalue = FALSE;
      result->any.is_rvalue = TRUE;
    }
  else
    {
      dang_compile_result_init_stack (result, stype, ret_var_id,
                                      FALSE, FALSE, FALSE);
    }

  dang_compile_result_steal_locks (result, container);
  if (flags->must_be_lvalue)
    {
      /* Setup callback functions */
      LValueData *lvdata = dang_new (LValueData, 1);
      result->stack.lvalue_callback = compile_lvalue_callback;
      lvdata->union_res = *container;
      lvdata->case_index = case_index;
      result->stack.callback_data = lvdata;
      result->stack.callback_data_destroy = free_lvalue_callback_data;
      result->any.is_lvalue = TRUE;
    }
  else
    {
      dang_compile_result_clear (container, builder);
    }

  return TRUE;
}

#if 0
static dang_boolean
compile_set__union (DangBuilder   *builder,
                    DangCompileResult     *container,
                    DangCompileResult     *member,
                    void                  *member_data,
                    DangCompileFlags      *flags,
                    DangCompileResult     *result)
{
  DangValueType *cont_type = container->any.return_type;
  DangValueTypeUnion *utype = (DangValueTypeUnion *) cont_type;
  unsigned case_index = DANG_POINTER_TO_UINT (member_data);
  DangValueType *stype = utype->cases[case_index].struct_type;
  DangCompileResult casted_cont;
  DangVarId ret_var_id;
  dang_assert (dang_value_type_is_union (cont_type));
  dang_compile_check_union_type (builder, container, utype, case_index);
  DANG_UNUSED (flags);

  /* Allocate the return variable. */
  ret_var_id = dang_builder_add_tmp (builder, stype);

  /* Add an alias to 'container'
     and compile an assignment (using the struct_type). */
  casted_cont = *container;
  casted_cont.any.return_type = stype;

  dang_builder_add_assign (builder, &casted_cont, member);
  dang_compile_result_init_void (result);
  return TRUE;
}
#endif

static DangError *
make_union_code_error (DangValueTypeUnion *u,
                       unsigned            actual_code,
                       unsigned            expected_code)
{
  return dang_error_new ("expected %s.%s got %s.%s",
                         u->base_type.full_name,
                         u->cases[expected_code].name,
                         u->base_type.full_name,
                         u->cases[actual_code].name);
}

static inline dang_boolean
check_union_code (DangValueTypeUnion *u,
                  unsigned            actual_code,
                  unsigned            expected_code,
                  DangError         **error)
{
  if (DANG_UNLIKELY (actual_code != expected_code))
    {
      *error = make_union_code_error (u, actual_code, expected_code);
      return FALSE;
    }
  return TRUE;
}

static DANG_SIMPLE_C_FUNC_DECLARE (simple_c__check_union_code1)
{
  DANG_UNUSED (rv_out);
  return check_union_code (func_data, *(uint8_t*)args[0], *(uint32_t*)args[1], error);
}
static DANG_SIMPLE_C_FUNC_DECLARE (simple_c__check_union_code2)
{
  DANG_UNUSED (rv_out);
  return check_union_code (func_data, *(uint16_t*)args[0], *(uint32_t*)args[1], error);
}
static DANG_SIMPLE_C_FUNC_DECLARE (simple_c__check_union_code4)
{
  DANG_UNUSED (rv_out);
  return check_union_code (func_data, *(uint32_t*)args[0], *(uint32_t*)args[1], error);
}

static unsigned read_code_1 (const void *a) { return * (const uint8_t *) a; }
static unsigned read_code_2 (const void *a) { return * (const uint16_t *) a; }
static unsigned read_code_4 (const void *a) { return * (const uint32_t *) a; }
static void write_code_1 (void *d, unsigned s) { *(uint8_t*)d = s; }
static void write_code_2 (void *d, unsigned s) { *(uint16_t*)d = s; }
static void write_code_4 (void *d, unsigned s) { *(uint32_t*)d = s; }

DangValueType *
dang_value_type_new_union (const char    *name,
                           unsigned       n_cases,
                           DangUnionCase *cases)
{
  DangValueTypeUnion *rv = dang_new0 (DangValueTypeUnion, 1);
  unsigned type_size = n_cases <= 256 ? 1 : n_cases <= 65536 ? 2 : 4;
  unsigned size = type_size;
  unsigned align = type_size;
  unsigned i;
  dang_boolean is_managed = FALSE;
  char *n;
  DangEnumValue *enum_values = dang_new (DangEnumValue, n_cases);
  DangValueType *enum_type;
  DangFunctionParam params[2];
  DangSignature *sig;
  DangSimpleCFunc check_union_sfunc;
  void (*write_code)(void *dst, unsigned code);
  for (i = 0; i < n_cases; i++)
    {
      enum_values[i].code = i;
      enum_values[i].name = cases[i].name;
    }

  n = dang_strdup_printf ("%s.type", name);
  enum_type = dang_value_type_new_enum (n, 0, n_cases, enum_values, NULL);
  dang_assert (enum_type != NULL);
  dang_free (n);

  switch (enum_type->sizeof_instance)
    {
    case 1:
      write_code = write_code_1;
      rv->read_code = read_code_1;
      check_union_sfunc = simple_c__check_union_code1;
      break;
    case 2:
      write_code = write_code_2;
      rv->read_code = read_code_2;
      check_union_sfunc = simple_c__check_union_code2;
      break;
    case 4:
      write_code = write_code_4;
      rv->read_code = read_code_4;
      check_union_sfunc = simple_c__check_union_code4;
      break;
    default:
      dang_assert_not_reached ();
    }

  /* Initialize case sizes and offsets to compute size/align of the
     overall union. */
  for (i = 0; i < n_cases; i++)
    {
      n = dang_strdup_printf ("%s.%s", name, cases[i].name);
      DangStructMember *mems = dang_new (DangStructMember, cases[i].n_members + 1);
      memcpy (mems + 1, cases[i].members, cases[i].n_members * sizeof(DangStructMember));
      mems[0].type = enum_type;
      mems[0].name = NULL;
      mems[0].has_default_value = TRUE;
      mems[0].default_value = dang_malloc (enum_type->sizeof_instance);
      write_code (mems[0].default_value, i);

      cases[i].struct_type
        = dang_value_type_new_struct (n, cases[i].n_members + 1, mems);

      align = DANG_MAX (align, cases[i].struct_type->alignof_instance);
      size = DANG_MAX (align, cases[i].struct_type->sizeof_instance);
      if (!is_managed)
        is_managed = cases[i].struct_type->init_assign != NULL;
    }
  size = DANG_ALIGN (size, align);

  rv->base_type.magic = DANG_VALUE_TYPE_MAGIC;
  rv->base_type.ref_count = 1;
  rv->base_type.full_name = dang_strdup (name);
  rv->base_type.sizeof_instance = size;
  rv->base_type.alignof_instance = align;

  if (is_managed)
    {
      rv->base_type.init_assign = init_assign__union;
      rv->base_type.assign = assign__union;
      rv->base_type.destruct = destruct__union;
    }
  rv->base_type.to_string = to_string__union;
  rv->n_cases = n_cases;
  rv->cases = cases;
  rv->enum_type = enum_type;
  dang_free (enum_values);
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].name = NULL;
  params[0].type = (DangValueType *) rv;
  params[1].dir = DANG_FUNCTION_PARAM_IN;
  params[1].name = NULL;
  params[1].type = dang_value_type_uint32 ();
  sig = dang_signature_new (NULL, 2, params);
  rv->check_union_code = dang_function_new_simple_c (sig, check_union_sfunc, rv, NULL);
  dang_signature_unref (sig);

  for (i = 0; i < n_cases; i++)
    dang_value_type_add_virtual_member (&rv->base_type,
                                        cases[i].name,
                                        DANG_MEMBER_COMPLETELY_PUBLIC,
                                        cases[i].struct_type,
                                        compile_access__union,
                                        DANG_UINT_TO_POINTER (i),
                                        NULL);

  rv->next_global_union = global_union_list;
  global_union_list = rv;

  return &rv->base_type;
}
dang_boolean   dang_value_type_is_union  (DangValueType *type)
{
  return type->to_string == to_string__union;
}


void _dang_union_cleanup (void)
{
  while (global_union_list)
    {
      DangValueTypeUnion *kill = global_union_list;
      unsigned i;
      global_union_list = kill->next_global_union;
      for (i = 0; i < kill->n_cases; i++)
        {
          dang_free (kill->cases[i].members);
          dang_free (kill->cases[i].name);
        }
      dang_function_unref (kill->check_union_code);
      dang_value_type_cleanup (&kill->base_type);
      dang_free (kill->cases);
      dang_free (kill->base_type.full_name);
      dang_free (kill);
    }
}
