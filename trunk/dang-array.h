
typedef struct _DangArray DangArray;
struct _DangArray
{
  DangTensor *tensor;
  unsigned ref_count;
  unsigned alloced;             /* allocated size of first dimension */
};

DangValueType *dang_value_type_array (DangValueType *element_type,
                                      unsigned       rank);

