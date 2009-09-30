
typedef struct _DangObject DangObject;

typedef struct _DangWeakRef DangWeakRef;
struct _DangWeakRef
{
  unsigned ref_count;
  void *object;
};

///struct _DangObjectFuncs
///{
///  void * (*object_ref) (void *);
///  void   (*object_unref) (void *);
///
///  /* optional */
///  DangWeakRef *(*object_weak_ref) (void *);
///  void         (*object_weak_unref) (DangWeakRef *);
///};
///


typedef struct _DangObjectClass DangObjectClass;
struct _DangObjectClass
{
  DangValueType *type;

  /* perhaps include common methods here eg to_string */
};

struct _DangObject
{
  DangObjectClass *the_class; /* NB: not 'class' b/c it's a c++ keyword */
  unsigned ref_count;
  DangWeakRef *weak_ref;

  /* members and mutable methods of derived classes follow */
};

typedef struct _DangValueTypeObject DangValueTypeObject;
struct _DangValueTypeObject
{
  DangValueType base_type;
  unsigned class_size;
  unsigned instance_size;             /* sizeof the actual object,
                                         NOT base_type.sizeof_instance,
                                         which is the size of a pointer. */
  void *the_class;
  void *prototype_instance;
  unsigned class_alloced;
  unsigned instance_alloced;
  dang_boolean subclassed;
  dang_boolean instantiated;
  dang_boolean compiled_virtuals;

  DangValueTypeObject *next_sibling, *prev_sibling;
  DangValueTypeObject *first_child, *last_child;

  /* Type,offset pairs of all instance members that require destruction */
  DangUtilArray non_memcpy_members;

  DangUtilArray mutable_fct_offsets;
};
DangValueType *dang_value_type_object (void);
dang_boolean   dang_value_type_is_object  (DangValueType *type);

DangValueType * dang_object_type_subclass (DangValueType *parent_type,
                                           const char    *full_name);

/* these can only be called until subclassed or instantiated. */

/* NB: the signature of the DangFunction here is different
   than the sig of that for dang_value_type_add_ctor() */
dang_boolean dang_object_add_constructor  (DangValueType *object_type,
                                           const char     *name,
                                           DangFunction   *func,
                                           DangError     **error);
dang_boolean dang_object_add_method       (DangValueType *object_type,
                                           const char     *name,
                                           DangMethodFlags flags,
                                           DangFunction  *func,
                                           DangError     **error);
dang_boolean dang_object_add_abstract_method(DangValueType *object_type,
                                           const char     *name,
                                           DangMethodFlags flags,
                                           DangSignature  *sig,
                                           DangError     **error);
dang_boolean dang_object_add_member       (DangValueType *object_type,
                                           const char     *name,
                                           DangMemberFlags flags,
                                           DangValueType  *member_type,
                                           const void    *default_value,
                                           DangError     **error);

/* NOTE: this api is for native objects only.
   To foreign-objects: perhaps you could move
   along to some other api?

   (if you ain't a foreigner, and just want to safely copy data,
   use the DangValueType's init_assign and assign functions.) */

void * dang_object_new   (DangValueType *type);
void * dang_object_ref   (void          *object);
void   dang_object_unref (void          *object);

/* We may actually inline these someday. */
#define dang_object_unref_inlined dang_object_unref
#define dang_object_ref_inlined dang_object_ref

/* Returns NULL if object is not of the target type. */
void * dang_object_cast  (DangValueType *target_type,
                          void          *object);

void dang_object_note_method_stubs (DangValueType *type,
                                    DangCompileContext *cc);
