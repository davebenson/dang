#include <stdlib.h>
#include <string.h>
#include "dang.h"
#include "magic.h"

static int
compare_code_to_enum_value (const void *a, const void *b)
{
  unsigned a_code = DANG_POINTER_TO_UINT (a);
  unsigned b_code = ((const DangEnumValue*)b)->code;
  return (a_code < b_code) ? -1 : (a_code > b_code) ? 1 : 0;
}

DangEnumValue *
dang_enum_lookup_value (DangValueTypeEnum *etype,
                        unsigned code)
{
  return bsearch (DANG_UINT_TO_POINTER (code),
                  etype->values_by_code,
                  etype->n_values,
                  sizeof (DangEnumValue),
                  compare_code_to_enum_value);
}

static int
compare_name_to_enum_value (const void *a, const void *b)
{
  const char* a_name = a;
  const char* b_name = ((const DangEnumValue*)b)->name;
  return strcmp (a_name, b_name);
}

DangEnumValue *
dang_enum_lookup_value_by_name (DangValueTypeEnum *etype,
                        const char *name)
{
  return bsearch ((void*) name,
                  etype->values_by_name,
                  etype->n_values,
                  sizeof (DangEnumValue),
                  compare_name_to_enum_value);
}

static char *
to_string__enum (DangValueType *type,
                 const void    *value)
{
  unsigned v;
  const DangEnumValue *val;
  DangValueTypeEnum *etype = (DangValueTypeEnum *) type;
  switch (type->sizeof_instance)
    {
    case 1: v = * (const uint8_t*) value; break;
    case 2: v = * (const uint16_t*) value; break;
    case 4: v = * (const uint32_t*) value; break;
    default: dang_assert_not_reached ();
    }
  val = dang_enum_lookup_value (etype, v);
  if (val == NULL)
    return dang_strdup_printf ("%s.*invalid*(%u)", type->full_name, v);
  else
    return dang_strdup_printf ("%s.%s", type->full_name, val->name);
}

static DangValueTypeEnum *global_enum_list = NULL;

static int
compare_enum_values_by_name (const void *a, const void *b)
{
  const DangEnumValue *ea = a;
  const DangEnumValue *eb = b;
  return strcmp (ea->name, eb->name);
}

static int
compare_enum_values_by_code (const void *a, const void *b)
{
  const DangEnumValue *ea = a;
  const DangEnumValue *eb = b;
  return (ea->code < eb->code) ? -1 : (ea->code > eb->code) ? 1 : 0;
}

static DANG_SIMPLE_C_FUNC_DECLARE(do_cast1)
{
  uint32_t v = * (uint32_t*) args[0];
  DangValueTypeEnum *etype = func_data;
  if (dang_enum_lookup_value (etype, v) == NULL)
    {
      dang_set_error (error, "no value %u for enum %s", v, etype->base_type.full_name);
      return FALSE;
    }
  * (uint8_t*) rv_out = v;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_cast2)
{
  uint32_t v = * (uint32_t*) args[0];
  DangValueTypeEnum *etype = func_data;
  if (dang_enum_lookup_value (etype, v) == NULL)
    {
      dang_set_error (error, "no value %u for enum %s", v, etype->base_type.full_name);
      return FALSE;
    }
  * (uint16_t*) rv_out = v;
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(do_cast4)
{
  uint32_t v = * (uint32_t*) args[0];
  DangValueTypeEnum *etype = func_data;
  if (dang_enum_lookup_value (etype, v) == NULL)
    {
      dang_set_error (error, "no value %u for enum %s", v, etype->base_type.full_name);
      return FALSE;
    }
  * (uint32_t*) rv_out = v;
  return TRUE;
}

static DangFunction *
get_cast_func__enum (DangValueType *target_type,
                     DangValueType *source_type)
{
  DangValueTypeEnum *etype = (DangValueTypeEnum *) target_type;
  DangSimpleCFunc cfunc;
  DangFunction *cast_func;
  DangSignature *sig;
  DangFunctionParam param;
  if (source_type != dang_value_type_uint32 ())
    return NULL;

  cfunc = etype->base_type.sizeof_instance == 1 ? do_cast1
        : etype->base_type.sizeof_instance == 2 ? do_cast2
        : do_cast4;
  param.dir = DANG_FUNCTION_PARAM_IN;
  param.name = NULL;
  param.type = source_type;
  sig = dang_signature_new (target_type, 1, &param);
  cast_func = dang_function_new_simple_c (sig, cfunc, etype, NULL);
  dang_signature_unref (sig);
  return cast_func;
}

DangValueType *
dang_value_type_new_enum (const char *name,
                          unsigned min_byte_size,
                          unsigned n_values,
                          DangEnumValue *values,
                          DangError **error)
{
  unsigned size = sizeof (DangValueTypeEnum)
                + sizeof (DangEnumValue) * 2 * n_values;
  unsigned i;
  DangEnumValue *by_name, *by_code;
  DangValueTypeEnum *rv;
  unsigned siz;
  unsigned max_code;
  char *at;
  if (n_values == 0)
    {
      dang_set_error (error, "enum %s must have at least one value, for 0",
                      name);
      return FALSE;
    }
  for (i = 0; i < n_values; i++)
    size += strlen (values[i].name) + 1;
  size += strlen (name) + 1;
  rv = dang_malloc (size);
  memset (rv, 0, sizeof (DangValueType));
  by_name = (DangEnumValue*)(rv + 1);
  by_code = by_name + n_values;
  at = (char*)(by_code + n_values);
  for (i = 0; i < n_values; i++)
    {
      by_name[i].code = values[i].code;
      by_name[i].name = strcpy (at, values[i].name);
      at = strchr (at, 0) + 1;
    }
  rv->base_type.full_name = strcpy (at, name);
  qsort (by_name, n_values, sizeof (DangEnumValue),
         compare_enum_values_by_name);
  for (i = 1; i < n_values; i++)
    if (strcmp (by_name[i-1].name, by_name[i].name) == 0)
      {
        dang_set_error (error, "enum %s defined value named '%s' twice",
                        name, by_name[i].name);
        dang_free (rv);
        return NULL;
      }
  memcpy (by_code, by_name, sizeof (DangEnumValue) * n_values);
  qsort (by_code, n_values, sizeof (DangEnumValue),
         compare_enum_values_by_code);
  for (i = 1; i < n_values; i++)
    if (by_code[i-1].code == by_code[i].code)
      {
        dang_set_error (error, "enum %s defined two values with code %u (named '%s' and '%s')",
                        name, by_code[i].code,
                        by_code[i-1].name, by_code[i].name);
        dang_free (rv);
        return NULL;
      }
  if (by_code[0].code != 0)
    {
      dang_set_error (error, "enum %s does not define a value for 0", name);
      dang_free (rv);
      return NULL;
    }
  max_code = (n_values == 0) ? 0 : by_code[n_values-1].code;
  if (max_code < 256)
    siz = 1;
  else if (max_code < (1<<16))
    siz = 2;
  else
    siz = 4;
  if (siz < min_byte_size)
    {
      dang_assert (min_byte_size == 2 || min_byte_size == 4);
      siz = min_byte_size;
    }

  rv->base_type.magic = DANG_VALUE_TYPE_MAGIC;
  rv->base_type.ref_count = 1;
  rv->base_type.sizeof_instance = siz;
  rv->base_type.alignof_instance = siz;
  rv->base_type.to_string = to_string__enum;
  rv->base_type.get_cast_func = get_cast_func__enum;
  rv->n_values = n_values;
  rv->values_by_name = by_name;
  rv->values_by_code = by_code;
  rv->next_global_enum = global_enum_list;
  global_enum_list = rv;
  return &rv->base_type;
}

dang_boolean
dang_value_type_is_enum (DangValueType *type)
{
  return type->to_string == to_string__enum;
}

static DANG_SIMPLE_C_FUNC_DECLARE(cast_enum8_to_uint32)
{
  DANG_UNUSED (error); DANG_UNUSED (func_data);
  * (uint32_t *) rv_out = * (uint8_t *) args[0];
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(cast_enum16_to_uint32)
{
  DANG_UNUSED (error); DANG_UNUSED (func_data);
  * (uint32_t *) rv_out = * (uint16_t *) args[0];
  return TRUE;
}
static DANG_SIMPLE_C_FUNC_DECLARE(cast_enum32_to_uint32)
{
  DANG_UNUSED (error); DANG_UNUSED (func_data);
  * (uint32_t *) rv_out = * (uint32_t *) args[0];
  return TRUE;
}

static DANG_FUNCTION_TRY_SIG_FUNC_DECLARE (try_sig__enum_to_uint32)
{
  DangSignature *sig;
  DangValueTypeEnum *etype;
  DangFunction *rv;
  DangFunctionParam param;
  DangSimpleCFunc cfunc;
  DANG_UNUSED (data);
  DANG_UNUSED (error);
  if (query->n_elements != 1
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return NULL;
  if (!dang_value_type_is_enum (query->elements[0].info.simple_input))
    return NULL;
  etype = (DangValueTypeEnum*) query->elements[0].info.simple_input;
  param.dir = DANG_FUNCTION_PARAM_IN;
  param.name = NULL;
  param.type = &etype->base_type;
  sig = dang_signature_new (dang_value_type_uint32 (), 1, &param);
  switch (etype->base_type.sizeof_instance)
    {
    case 1: cfunc = cast_enum8_to_uint32; break;
    case 2: cfunc = cast_enum16_to_uint32; break;
    case 4: cfunc = cast_enum32_to_uint32; break;
    }
  rv = dang_function_new_simple_c (sig, cfunc, NULL, NULL);
  dang_signature_unref (sig);
  return rv;
}

static DANG_SIMPLE_C_FUNC_DECLARE(do_enum_to_string)
{
  DangEnumValue *ev;
  DangValueTypeEnum *etype = func_data;
  unsigned v;
  switch (etype->base_type.sizeof_instance)
    {
    case 1: v = * (const uint8_t*) args[0]; break;
    case 2: v = * (const uint16_t*) args[0]; break;
    case 4: v = * (const uint32_t*) args[0]; break;
    }
  ev = dang_enum_lookup_value (etype, v);
  if (ev == NULL)               /* shouldn't happen */
    {
      dang_set_error (error, "no value %u for enum %s",
                      v, etype->base_type.full_name);
      return FALSE;
    }
  * (DangString **) rv_out = dang_string_new (ev->name);
  return TRUE;
}

static DANG_FUNCTION_TRY_SIG_FUNC_DECLARE (try_sig__enum_to_string)
{
  DangSignature *sig;
  DangValueTypeEnum *etype;
  DangFunction *rv;
  DangFunctionParam param;
  DANG_UNUSED (data);
  DANG_UNUSED (error);
  if (query->n_elements != 1
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT)
    return NULL;
  if (!dang_value_type_is_enum (query->elements[0].info.simple_input))
    return NULL;
  etype = (DangValueTypeEnum*) query->elements[0].info.simple_input;
  param.dir = DANG_FUNCTION_PARAM_IN;
  param.name = NULL;
  param.type = &etype->base_type;
  sig = dang_signature_new (dang_value_type_string (), 1, &param);
  rv = dang_function_new_simple_c (sig, do_enum_to_string, etype, NULL);
  dang_signature_unref (sig);
  return rv;
}

static DangFunction *try_sig__binary_comparators (DangMatchQuery *query,
                                                   DangSimpleCFunc func1,
                                                   DangSimpleCFunc func2,
                                                   DangSimpleCFunc func4,
                                                   DangError **error)
{
  DangFunction *rv;
  DangFunctionParam params[2];
  DangSignature *sig;
  DangSimpleCFunc func;
  DANG_UNUSED (error);
  if (query->n_elements != 2
   || query->elements[0].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || query->elements[1].type != DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT
   || query->elements[0].info.simple_input != query->elements[1].info.simple_input
   || !dang_value_type_is_enum (query->elements[0].info.simple_input))
    return NULL;
  params[0].dir = DANG_FUNCTION_PARAM_IN;
  params[0].name = NULL;
  params[0].type = query->elements[0].info.simple_input;
  params[1] = params[0];
  sig = dang_signature_new (dang_value_type_boolean (), 2, params);
  switch (query->elements[0].info.simple_input->sizeof_instance)
    {
    case 1: func = func1; break;
    case 2: func = func2; break;
    case 4: func = func4; break;
    default: dang_assert_not_reached ();
    }
  rv = dang_function_new_simple_c (sig, func, NULL, NULL);
  dang_signature_unref (sig);
  return rv;
}

#define DEFINE_BIN_CMP(func_name, ctype, cmp)                  \
static DANG_SIMPLE_C_FUNC_DECLARE(func_name)                   \
{                                                              \
  DANG_UNUSED (error); DANG_UNUSED (func_data);                \
  *(char*)rv_out = (*(ctype*) args[0] cmp *(ctype*) args[1]);  \
  return TRUE;                                                 \
}
DEFINE_BIN_CMP(enum_op_equal_1, uint8_t, ==)
DEFINE_BIN_CMP(enum_op_equal_2, uint16_t, ==)
DEFINE_BIN_CMP(enum_op_equal_4, uint32_t, ==)
DEFINE_BIN_CMP(enum_op_not_equal_1, uint8_t, !=)
DEFINE_BIN_CMP(enum_op_not_equal_2, uint16_t, !=)
DEFINE_BIN_CMP(enum_op_not_equal_4, uint32_t, !=)

static DANG_FUNCTION_TRY_SIG_FUNC_DECLARE (try_sig__operator_equal)
{
  DANG_UNUSED (data);
  return try_sig__binary_comparators (query,
                                      enum_op_equal_1,
                                      enum_op_equal_2,
                                      enum_op_equal_4,
                                      error);
}
static DANG_FUNCTION_TRY_SIG_FUNC_DECLARE (try_sig__operator_not_equal)
{
  DANG_UNUSED (data);
  return try_sig__binary_comparators (query,
                                      enum_op_not_equal_1,
                                      enum_op_not_equal_2,
                                      enum_op_not_equal_4,
                                      error);
}

void _dang_enum_init (DangNamespace *the_ns)
{
  /* register cast to uint32 */
  DangFunctionFamily *ff;

  ff = dang_function_family_new_variadic_c ("enum_to_uint32",
                                            try_sig__enum_to_uint32,
                                            NULL, NULL);
  if (!dang_namespace_add_function_family (the_ns, "operator_cast__uint32",
                                      ff, NULL))
    dang_assert_not_reached ();
  dang_function_family_unref (ff);

  /* register to_string() function */
  ff = dang_function_family_new_variadic_c ("enum_to_string",
                                            try_sig__enum_to_string,
                                            NULL, NULL);
  if (!dang_namespace_add_function_family (the_ns, "to_string", ff, NULL))
    dang_assert_not_reached ();
  dang_function_family_unref (ff);

  /* register operator_equals() and operator_not_equals() function */
  ff = dang_function_family_new_variadic_c ("operator_equal",
                                            try_sig__operator_equal,
                                            NULL, NULL);
  if (!dang_namespace_add_function_family (the_ns, "operator_equal", ff, NULL))
    dang_assert_not_reached ();
  dang_function_family_unref (ff);
  ff = dang_function_family_new_variadic_c ("operator_not_equal",
                                            try_sig__operator_not_equal,
                                            NULL, NULL);
  if (!dang_namespace_add_function_family (the_ns, "operator_not_equal", ff, NULL))
    dang_assert_not_reached ();
  dang_function_family_unref (ff);
}

void _dang_enum_cleanup (void)
{
  while (global_enum_list)
    {
      DangValueTypeEnum *kill = global_enum_list;
      global_enum_list = kill->next_global_enum;
      dang_free (kill);
    }
}
