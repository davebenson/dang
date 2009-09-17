typedef struct _DangUnionCase DangUnionCase;
typedef struct _DangValueTypeUnion DangValueTypeUnion;

struct _DangUnionCase
{
  char *name;
  unsigned n_members;
  DangStructMember *members;

  /* to be filled out be dang_value_type_new_union() */
  DangValueType *struct_type;
};

struct _DangValueTypeUnion
{
  DangValueType base_type;
  unsigned n_cases;
  DangUnionCase *cases;
  DangValueType *enum_type;
  unsigned (*read_code)(const void *value);
  void     (*write_code)(void *value, unsigned code);
  DangFunction *check_union_code;
  DangValueTypeUnion *next_global_union;
};

DangValueType *dang_value_type_new_union (const char    *name,
                                          unsigned       n_cases,
                                          DangUnionCase *cases);

dang_boolean   dang_value_type_is_union  (DangValueType *type);
