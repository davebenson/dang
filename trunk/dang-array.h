
typedef struct _DangArray DangArray;
typedef struct _DangArrayClass DangArrayClass;

struct _DangArrayClass
{
  DangObjectClass base;
};

struct _DangArray
{
  DangObject base;
  DangArray *tensor;
  unsigned alloced;             /* allocated size of first dimension */
};

typedef struct _DangValueTypeArray DangValueTypeArray;
struct _DangValueTypeArray
{
  DangValueType base_type;
  DangValueType *element_type;
  unsigned rank;
  DangValueType *tensor_type;

  /* cached */
  DangFunction *to_string_function;

  DangValueTypeArray *left, *right, *parent;
  dang_boolean is_red;

  DangValueIndexInfo index_infos[2];
};

DangValueType *dang_value_type_array (DangValueType *element_type,
                                      unsigned       rank);

