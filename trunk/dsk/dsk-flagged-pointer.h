/* DskFlaggedPointer: a pointer + 2 bits.

   In order for this trick to work, your pointer must be aligned.
   Note that many string pointers are note aligned.  Any objects,
   pointers to integers, etc are almost certainly aligned.

   Usually this is pretty silly, but we do it to reduce the
   overhead of our dns implementation. */

/* NOTE: this file is not part of the public DSK api.
   subject to change or deletion */


typedef union { size_t mangled_ptr; } DskFlaggedPointer;

#define DSK_FLAGGED_POINTER_INIT(ptr, flag1, flag2) \
  { DSK_FLAGGED_POINTER_MAKE_MANGLED(ptr, flag1, flag2) }
#define DSK_FLAGGED_POINTER_SET(fp, ptr, flag1, flag2) \
  do{(fp).mangled_ptr = DSK_FLAGGED_POINTER_MAKE_MANGLED(ptr, flag1, flag2);}while(0)
#define DSK_FLAGGED_POINTER_PTR(fp)  \
  ((void*)((fp).mangled_ptr & ~((size_t)3)))
#define DSK_FLAGGED_POINTER_FLAG1(fp)  \
  (((fp).mangled_ptr & 1))
#define DSK_FLAGGED_POINTER_FLAG2(fp)  \
  (((fp).mangled_ptr & 2) == 2)

#define DSK_FLAGGED_POINTER_PTR(fp)  \
  ((void*)((fp).mangled_ptr & ~((size_t)3)))
#define DSK_FLAGGED_POINTER_FLAG1(fp)  \
  (((fp).mangled_ptr & 1))
#define DSK_FLAGGED_POINTER_FLAG2(fp)  \
  (((fp).mangled_ptr & 2) == 2)


#define DSK_FLAGGED_POINTER_SET_PTR(fp, ptr) \
  do {(fp).mangled_ptr = ((fp).mangled_ptr & 3) | (size_t)(ptr);}while(0)
#define DSK_FLAGGED_POINTER_SET_FLAG1(fp, flag1) \
  if (flag1) DSK_FLAGGED_POINTER_MARK_FLAG1(fp) else DSK_FLAGGED_POINTER_CLEAR_FLAG1 (fp)



#define DSK_FLAGGED_POINTER_CLEAR_FLAG1(fp)  \
  ((fp).mangled_ptr &= ~(size_t)1)
#define DSK_FLAGGED_POINTER_MARK_FLAG1(fp)  \
  ((fp).mangled_ptr |= 1)
#define DSK_FLAGGED_POINTER_CLEAR_FLAG2(fp)  \
  ((fp).mangled_ptr &= ~(size_t)2)
#define DSK_FLAGGED_POINTER_MARK_FLAG2(fp)  \
  ((fp).mangled_ptr |= 2)


#define DSK_FLAGGED_POINTER_MAKE_MANGLED(ptr, flag1, flag2) \
  (size_t)(ptr) | (((flag1)?1:0) | ((flag2)?2:0))
