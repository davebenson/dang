/* pontification only -- use case for abstract type system */


/* weak pointers:  pointers that become NULL when the object is finalized. */
/*     these have a single virtual member "ptr"
       that is suitable for getting or setting. */


/* This is how DangObject implements weak pointers:  they are a 
 * pointer to this structure.
 * The accessors of this structure will unreference it if they
 * encounter a NULL version of it.   (But I suspect it'll come up fairly rarely, maybe)
 */
struct _DangObjectWeakPtr
{
  unsigned ref_count;
  DangObject *object;
};

struct _DangWeakPtrFuncs
{
  size_t sizeof_instance;
  size_t alignof_instance;
  DangType *(*create_weak_ptr_type)(DangType *object_type);
  void     *(*access_weak_ptr)     (void     *ptr_weak_ptr);
  void      (*assign_weak_ptr)     (void     *ptr_weak_ptr,
                                    void     *object_or_null);
};

void              dang_object_register_weak_ptr (DangValueType    *base_object_type,
                                                 DangWeakPtrFuncs *funcs);

DangValueType         *dang_weak_pointer_type_get (DangValueType *object_type);
DangWeakPtrFuncs *dang_weak_ptr_funcs_get (DangValueType *weak_ptr_type);

/*
Assignments between weak[[Object]], Object
       
       a = b;         /// if both weak refs or pointers
       a.ptr = b;     /// a weak, b strong
       a = b.ptr;     /// a strong, b weak
 */



