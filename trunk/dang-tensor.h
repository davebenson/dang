typedef struct _DangValueTypeTensor DangValueTypeTensor;
struct _DangValueTypeTensor
{
  DangValueType base_type;
  DangValueType *element_type;
  unsigned rank;

  /* cached */
  DangFunction *to_string_function;

  DangValueTypeTensor *left, *right, *parent;
  dang_boolean is_red;

  DangValueIndexInfo index_infos[2];
};


DangValueType *dang_value_type_tensor (DangValueType *element_type,
                                       unsigned rank);
dang_boolean dang_value_type_is_tensor (DangValueType *type);

#define dang_value_type_array(element_type) \
  dang_value_type_tensor(element_type,1)
#define dang_value_type_vector dang_value_type_array
#define dang_value_type_matrix(element_type) \
  dang_value_type_tensor(element_type,2)




typedef struct _DangTensor DangTensor;
struct _DangTensor
{
  void *data;

  /* the actual length of this array is the rank of the tensor */
  unsigned sizes[1];
};

void _dang_tensor_init (DangNamespace *the_ns);

typedef struct _DangMatrix DangMatrix;
struct _DangMatrix
{
  void *data;
  unsigned n_rows, n_cols;
};

typedef struct _DangVector DangVector;
struct _DangVector
{
  void *data;
  unsigned len;
  unsigned alloced;             /* different than higher rank tensors!!! */
};

#define DANG_TENSOR_SIZEOF(rank)                                              \
( ((rank)==1) ? sizeof(DangVector)                                            \
              : DANG_ALIGN((sizeof(DangTensor) + ((rank)-1)*sizeof(unsigned)),\
                           DANG_ALIGNOF_POINTER) )
