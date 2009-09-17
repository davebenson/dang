
typedef struct _DangEnumValue DangEnumValue;
typedef struct _DangValueTypeEnum DangValueTypeEnum;

struct _DangEnumValue
{
  const char *name;
  unsigned code;
};

struct _DangValueTypeEnum
{
  DangValueType base_type;
  unsigned n_values;
  DangEnumValue *values_by_code;
  DangEnumValue *values_by_name;

  DangValueTypeEnum *next_global_enum;
};

/* does NOT take ownership (sigh) */
DangValueType *dang_value_type_new_enum (const char *name,
                                         unsigned min_byte_size,
                                         unsigned n_values,
                                         DangEnumValue *values,
                                         DangError **error);
dang_boolean dang_value_type_is_enum (DangValueType *type);

DangEnumValue *dang_enum_lookup_value (DangValueTypeEnum *,
                                       unsigned           code);
DangEnumValue *dang_enum_lookup_value_by_name (DangValueTypeEnum *,
                                               const char        *name);
