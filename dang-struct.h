
typedef struct _DangStructMember DangStructMember;
typedef struct _DangValueTypeStruct DangValueTypeStruct;

struct _DangStructMember
{
  DangValueType *type;
  char *name;
  unsigned offset;

  dang_boolean has_default_value;
  void *default_value;
};

struct _DangValueTypeStruct
{
  DangValueType base_type;

  unsigned n_members;
  DangStructMember *members;

  DangValueTypeStruct *prev_struct;
};

/* NOTE: takes ownership! (of 'name' and 'members'!) 
   this function will handle computing the offsets */
DangValueType *dang_value_type_new_struct (char *name,
                                           unsigned    n_members,
				           DangStructMember *members);
dang_boolean dang_value_type_is_struct (DangValueType*);



/* Used by dang_value_type_new_union() */
DangValueType *_dang_value_type_new_struct_for_union (unsigned type_reserved,
                                                      char *name,
                                                      unsigned    n_members,
				                      DangStructMember *members);
