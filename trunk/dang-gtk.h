
struct _DangValueTypeGObject
{
  DangValueType base_type;
  GType gtype;
};

DangValueType *dang_value_type_gobject (GType type,
                                        DangValueType *parent_type);

DangValueType *dang_value_type_gtk_widget  (void);
DangValueType *dang_value_type_gtk_window  (void);
DangValueType *dang_value_type_gtk_label   (void);
DangValueType *dang_value_type_gtk_button  (void);

void dang_init_gtk (void);
