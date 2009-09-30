
typedef struct _DangValueType DangValueType;
typedef struct _DangValueMember DangValueMember;
typedef struct _DangValueMethod DangValueMethod;
typedef struct _DangValueElement DangValueElement;
typedef struct _DangValueInternals DangValueInternals;
typedef struct _DangValueIndexInfo DangValueIndexInfo;
typedef struct _DangValueObjectFuncs DangValueObjectFuncs;

typedef void (*DangValueAssignFunc) (DangValueType *type,
                                     void          *dst,
	                             const void    *src);
typedef char*(*DangValueToStringFunc)(DangValueType *type,
	                              const void    *value);

typedef DangFunction *(*DangValueGetCastFunc)    (DangValueType *target_type,
                                                  DangValueType *source_type);

struct _DangValueInternals
{
  DangValueElement *element_tree;       /* tree (by name) of members and methods */
  DangValueElement *ctor_tree;          /* of constructors, by tag (or empty string) */
  DangValueType *parent;                /* for object types */
  dang_boolean is_templated;            /* contains a template_param somewhere inside */
  DangValueIndexInfo *index_infos;
};

struct _DangValueType
{
  unsigned magic;
  unsigned ref_count;           /* symbolic gesture:  types are permanent for now */
  char *full_name;
  size_t sizeof_instance;
  size_t alignof_instance;

  /* if NULL, use memcpy();
     init_assign() assumes that 'dst' is uninitialized memory.
     assign() assumes it is a valid value. */
  DangValueAssignFunc init_assign, assign;

  /* if NULL, no destruct() */
  void (*destruct)(DangValueType *type,
                   void          *value);

  /* Fallback implementations of compare and hash,
     for fast tree and hash-table implementations. */
  int (*compare)   (DangValueType *type,
                    const void    *a,
                    const void    *b);
  uint32_t (*hash) (DangValueType *type,
                    const void    *a);
  dang_boolean (*equal) (DangValueType *type,
                         const void    *a,
                         const void    *b);

  DangValueToStringFunc to_string;

  const char *cast_func_name;     /* function to call to cast to this type */

  DangValueGetCastFunc get_cast_func;

  /* private */
  DangValueInternals internals;
};

#define DANG_VALUE_INTERNALS_INIT  {NULL,NULL,NULL,0,NULL} /* same as zeroing it */

DangValueType *dang_value_type_int8(void);
DangValueType *dang_value_type_uint8(void);
DangValueType *dang_value_type_int16(void);
DangValueType *dang_value_type_uint16(void);
DangValueType *dang_value_type_int32(void);
DangValueType *dang_value_type_uint32(void);
DangValueType *dang_value_type_int64(void);
DangValueType *dang_value_type_uint64(void);
DangValueType *dang_value_type_float(void);
DangValueType *dang_value_type_double(void);
DangValueType *dang_value_type_string(void);
DangValueType *dang_value_type_char(void);
DangValueType *dang_value_type_integer(void);
DangValueType *dang_value_type_error(void);
DangValueType *dang_value_type_void(void);

/* NOTE: unlike in c, the boolean type in dang is a single byte */
DangValueType *dang_value_type_boolean(void);

DangValueType *dang_value_type_reserved_pointer (void);
DangValueType *dang_value_type_type (void);

dang_boolean   dang_value_type_is_a (DangValueType *type,
                                     DangValueType *is_a_type);
char *dang_value_to_string (DangValueType *type,
                            const void    *value);

dang_boolean dang_value_type_is_autocast (DangValueType *lvalue,
                                          DangValueType *rvalue);
typedef enum
{
  DANG_VALUE_MEMBER_TYPE_SIMPLE,
  DANG_VALUE_MEMBER_TYPE_VIRTUAL
} DangValueMemberType;

/* functions for virtual members */
/* NOTE: function is responsible for clearing 'container' */
typedef dang_boolean (*DangCompileVirtualMember)
                                           (DangBuilder   *builder,
                                            DangCompileResult     *container,
                                            void                  *member_data,
                                            DangCompileFlags      *flags,
                                            DangCompileResult     *result);


typedef enum
{
  DANG_MEMBER_PUBLIC_READABLE    = (1<<0),
  DANG_MEMBER_PUBLIC_WRITABLE    = (1<<1),
  DANG_MEMBER_PROTECTED_READABLE = (1<<2),
  DANG_MEMBER_PROTECTED_WRITABLE = (1<<3),
  DANG_MEMBER_PRIVATE_READABLE   = (1<<4),
  DANG_MEMBER_PRIVATE_WRITABLE   = (1<<5),
} DangMemberFlags;
#define DANG_MEMBER_COMPLETELY_PUBLIC    0x3f

struct _DangValueMember
{
  DangValueMemberType type;
  DangMemberFlags flags;
  DangValueType *member_type;
  union {
    struct {
      dang_boolean dereference;
      unsigned offset;
    } simple;
    struct {
      DangCompileVirtualMember compile;
      void *member_data;
      DangDestroyNotify member_data_destroy;
    } virt;
  } info;
};

void *dang_value_copy (DangValueType *type,
                       const void    *orig_value);
void dang_value_bulk_copy (DangValueType *type,
                           void          *dst,
                           const void    *src,
                           unsigned       N);
void dang_value_init_assign (DangValueType *type,
                             void          *dst,
                             const void    *src);
void dang_value_assign      (DangValueType *type,
                             void          *dst,
                             const void    *src);
void dang_value_destroy     (DangValueType *type,
                             void          *value);

typedef enum
{
  DANG_VALUE_ELEMENT_TYPE_MEMBER,
  DANG_VALUE_ELEMENT_TYPE_METHOD,
  DANG_VALUE_ELEMENT_TYPE_CTOR
} DangValueElementType;
const char *dang_value_element_type_name (DangValueElementType);
struct _DangValueElement
{
  DangValueElementType element_type;
  char *name;
  union {
    DangUtilArray methods;          /* of DangValueMethod */
    DangValueMember member;
    DangFunctionFamily *ctor;   /* of DangFunction */
  } info;

  /* private */
  DangValueElement *left,*right,*parent;
  dang_boolean is_red;
};

/* This looks up a method or member */
DangValueElement  *dang_value_type_lookup_element(DangValueType *type,
                                                  const char    *name,
                                                  dang_boolean   recurse,
                                                  DangValueType **base_type_out);

/* name may be NULL */
DangValueElement  *dang_value_type_lookup_ctor   (DangValueType *type,
                                                  const char    *name);


/* methods */
typedef enum
{
  DANG_METHOD_FINAL           = (1<<0),
  DANG_METHOD_ABSTRACT        = (1<<1),
  DANG_METHOD_MUTABLE         = (1<<2),
  DANG_METHOD_STATIC          = (1<<3),
  DANG_METHOD_PRIVATE         = (1<<4),
  DANG_METHOD_PROTECTED       = (1<<5),
  DANG_METHOD_PUBLIC          = (1<<6),
  DANG_METHOD_PUBLIC_READONLY = (1<<7),
} DangMethodFlags;

typedef void (*DangMethodCompileFunc) (DangValueType *type,
                                       DangValueMethod *method,
                                       DangBuilder *builder,
                                       DangCompileResult *instance,
                                       DangCompileResult *func_out);

struct _DangValueMethod
{
  DangSignature *sig;
  DangValueType *method_func_type;
  DangMethodFlags flags;
  DangFunction *func;           /* for final functions*/

  DangFunction *get_func;       /* for virtual and mutable functions */
  void *method_data;
  unsigned offset;              /* for use by dang-object */
  DangDestroyNotify method_data_destroy;
};

/* value types only interesting during compilation */
DangValueType *dang_value_type_namespace ();

dang_boolean     dang_value_type_find_method (DangValueType  *type,
                                              const char     *name,
                                              dang_boolean    has_object,
                                              DangMatchQuery *query,
                                              DangValueType   **method_type_out,
                                              DangValueElement **elt_out,
                                              unsigned *index_out,
                                              DangError     **error);

dang_boolean     dang_value_type_find_method_by_sig
                                             (DangValueType  *type,
                                              const char     *name,
                                              DangMethodFlags flags,
                                              DangSignature  *sig,
                                              DangValueType   **method_type_out,
                                              DangValueElement **elt_out,
                                              unsigned *index_out,
                                              DangError     **error);

/* protected */
void dang_value_type_add_simple_member (DangValueType *type,
                                        const char    *name,
                                        DangMemberFlags flags,
                                        DangValueType *member_type,
                                        dang_boolean   dereference,
                                        unsigned       offset);
void dang_value_type_add_virtual_member(DangValueType *type,
                                        const char    *name,
                                        DangMemberFlags flags,
                                        DangValueType *member_type,
                                        DangCompileVirtualMember compile,
                                        void          *member_data,
                                        DangDestroyNotify destroy);
void dang_value_type_add_constant_method(DangValueType *type,
                                         const char    *name,
                                         DangMethodFlags flags,
                                         DangFunction  *func);
void dang_value_type_add_simple_virtual_method
                                        (DangValueType *type,
                                         const char    *name,
                                         DangMethodFlags flags,
                                         DangSignature  *sig,
                                         unsigned       class_offset);
void dang_value_type_add_simple_mutable_method
                                        (DangValueType *type,
                                         const char    *name,
                                         DangMethodFlags flags,
                                         DangSignature  *sig,
                                         unsigned       instance_offset);
void dang_value_type_add_ctor           (DangValueType *type,
                                         const char    *name,
                                         DangFunction  *ctor);
DangFunctionFamily *dang_value_type_get_ctor (DangValueType *type,
                                              const char    *name);


/* --- Indexing methods --- */
typedef dang_boolean (*DangValueIndexGetFunc) (DangValueIndexInfo *info,
                                               void          *container,
                                               const void   **indices,
                                               void          *rv_out,
                                               dang_boolean   may_create,
                                               DangError    **error);
typedef dang_boolean (*DangValueIndexSetFunc) (DangValueIndexInfo *info,
                                               void          *container,
                                               const void   **indices,
                                               const void    *element_value,
                                               dang_boolean   may_create,
                                               DangError    **error);
typedef dang_boolean (*DangValueIndexGetPtrFunc)(DangValueIndexInfo *info,
                                               void          *container,
                                               const void   **indices,
                                               void         **rv_ptr_out,
                                               dang_boolean   may_create,
                                               DangError    **error);
struct _DangValueIndexInfo
{
  DangValueType *owner;
  unsigned n_indices;
  DangValueType **indices;
  DangValueType *element_type;
  DangValueIndexGetFunc get;
  DangValueIndexSetFunc set;
  DangValueIndexGetPtrFunc get_ptr;     /* optional */

  DangValueIndexInfo *next;
};


/* function declaration macros */
#define DANG_VALUE_COMPARE_FUNC_DECLARE(func_name) \
  int func_name(DangValueType *type, const void *a, const void *b)
#define DANG_VALUE_HASH_FUNC_DECLARE(func_name) \
  uint32_t func_name (DangValueType *type, const void *a)
#define DANG_VALUE_EQUAL_FUNC_DECLARE(func_name) \
  dang_boolean func_name(DangValueType *type, const void *a, const void *b)

void _dang_value_type_debug_init (void);

/* used at termination to cleanup type-related data */
void dang_value_type_cleanup (DangValueType *);
