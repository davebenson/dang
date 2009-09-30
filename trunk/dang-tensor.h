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

#define dang_value_type_vector(element_type) \
  dang_value_type_tensor(element_type,1)
#define dang_value_type_matrix(element_type) \
  dang_value_type_tensor(element_type,2)




typedef struct _DangTensor DangTensor;
struct _DangTensor
{
  void *data;

  unsigned ref_count;

  /* the actual length of this array is the rank of the tensor */
  unsigned sizes[1];
};

/* functions predominately useful from dang-array */
void dang_tensor_unref (DangValueType *tensor_type,
                        DangTensor *tensor);

char * dang_tensor_to_string (DangValueType *type,
                              DangTensor    *tensor);
void dang_tensor_oob_error (DangError **error,
                            unsigned    which_index,
                            unsigned    dim,
                            unsigned    index);
void _dang_tensor_init (DangNamespace *the_ns);

typedef struct _DangMatrix DangMatrix;
struct _DangMatrix
{
  void *data;
  unsigned ref_count;
  unsigned n_rows, n_cols;
};

typedef struct _DangVector DangVector;
struct _DangVector
{
  void *data;
  unsigned ref_count;
  unsigned len;
};

DangTensor *dang_tensor_empty (void);
#define dang_vector_empty()  ((DangVector*)dang_tensor_empty())
#define dang_matrix_empty()  ((DangMatrix*)dang_tensor_empty())

#define DANG_TENSOR_SIZEOF(rank)                                              \
(DANG_ALIGN((sizeof(DangTensor) + ((rank)-1)*sizeof(unsigned)), DANG_ALIGNOF_POINTER) )
