
typedef struct _DangArray DangArray;

struct _DangArray
{
  DangTensor *tensor;
  unsigned ref_count;
  unsigned alloced;             /* allocated size of first dimension */
};

typedef struct _DangValueTypeArray DangValueTypeArray;
struct _DangValueTypeArray
{
  DangValueType base_type;
  DangValueType *element_type;
  unsigned rank;
  DangValueType *tensor_type;

  DangValueTypeArray *left, *right, *parent;
  dang_boolean is_red;

  DangValueIndexInfo index_infos[2];
};

DangValueType *dang_value_type_array (DangValueType *element_type,
                                      unsigned       rank);
dang_boolean   dang_value_type_is_array (DangValueType *type);

void _dang_array_init (DangNamespace *ns);
