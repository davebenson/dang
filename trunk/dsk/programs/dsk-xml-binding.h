
struct _DskXmlBindingType
{
  dsk_boolean is_fundamental;
  dsk_boolean is_static;

  unsigned sizeof_type;
  unsigned alignof_type;

  /* virtual functions */
  dsk_boolean (*parse)(DskXmlBindingType *type,
                       DskXml            *to_parse,
		       void              *out,
		       DskError         **error);
  DskXml  *   (*to_xml)(DskXmlBindingType *type,
                        const char        *data,
		        DskError         **error);
  void        (*clear) (DskXmlBindingType *type,
		        void              *out);
};

typedef struct _DskXmlBinding DskXmlBinding;

void dsk_xml_binding_register_type (DskXmlBinding     *binding,
                                    const char        *type_name,
                                    DskXmlBindingType *type);

/* --- for generated code --- */
dsk_boolean dsk_xml_binding_struct_parse (DskXmlBindingType *type,
		                          DskXml            *to_parse,
		                          void              *out,
		                          DskError         **error);
DskXml  *   dsk_xml_binding_struct_to_xml(DskXmlBindingType *type,
		                          const char        *data,
		                          DskError         **error);
void        dsk_xml_binding_struct_clear (DskXmlBindingType *type,
		                          void              *out);
