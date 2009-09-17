
typedef struct _DangForeignObjectFuncs DangForeignObjectFuncs;
struct _DangForeignObjectFuncs
{
  dang_boolean (*is_a) (DangValueType *object_type,
                        void          *instance,
                        DangValueType *is_a_type);
  void        *(*ref)  (DangValueType *object_type,
                        void          *instance);
  void         (*unref)(DangValueType *object_type,
                        void          *instance);

};
DangValueType *dang_value_type_foreign_object (const char *name,
                                               DangForeignObjectFuncs *funcs);
DangValueType *dang_value_type_foreign_object
