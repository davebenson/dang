
typedef struct _DskXmlBindingType DskXmlBindingType;
typedef struct _DskXmlBinding DskXmlBinding;
typedef struct _DskXmlBindingStructMember DskXmlBindingStructMember;
typedef struct _DskXmlBindingTypeStruct DskXmlBindingTypeStruct;
typedef struct _DskXmlBindingUnionCase DskXmlBindingUnionCase;
typedef struct _DskXmlBindingTypeUnion DskXmlBindingTypeUnion;

typedef enum
{
  DSK_XML_BINDING_REQUIRED,
  DSK_XML_BINDING_OPTIONAL,
  DSK_XML_BINDING_REPEATED,
  DSK_XML_BINDING_REQUIRED_REPEATED
} DskXmlBindingQuantity;


struct _DskXmlBindingType
{
  dsk_boolean is_fundamental;
  dsk_boolean is_static;
  dsk_boolean is_struct;

  unsigned sizeof_type;
  unsigned alignof_type;
  char *name;

  /* virtual functions */
  dsk_boolean (*parse)(DskXmlBindingType *type,
                       DskXml            *to_parse,
		       void              *out,
		       DskError         **error);
  DskXml   *  (*to_xml)(DskXmlBindingType *type,
                        const char        *data,
		        DskError         **error);
  void        (*clear) (DskXmlBindingType *type,
		        void              *out);
};


struct _DskXmlBindingNamespaceEntry
{
  char *name;
  DskXmlBindingNamespace *ns;
  DskXmlBindingType *type;
};
struct _DskXmlBindingNamespace
{
  dsk_boolean is_static;
  char *name;
  unsigned n_entries;
  DskXmlBindingNamespaceEntry *entries;
};


DskXmlBinding *dsk_xml_binding_new (void);
void           dsk_xml_binding_add_searchpath (DskXmlBinding *binding,
                                               const char    *path);
DskXmlBindingNamespace*
               dsk_xml_binding_get_ns         (DskXmlBinding *binding,
                                               const char    *name,
                                               DskError     **error);

struct _DskXmlBindingStructMember
{
  DskXmlBindingQuantity quantity;
  char *name;
  DskXmlBindingType *type;
  unsigned quantifier_offset;
  unsigned offset;
};
struct _DskXmlBindingTypeStruct
{
  DskXmlBindingType base_type;
  unsigned n_members;
  DskXmlBindingStructMember *members;
};

struct _DskXmlBindingUnionCase
{
  char *name;
  dsk_boolean elide_struct_outer_tag;
  DskXmlBindingType *type;
};
struct _DskXmlBindingTypeUnion
{
  DskXmlBindingType base_type;
  unsigned variant_offset;
  unsigned n_cases;
  DskXmlBindingUnionCase *cases;
};


/* --- fundamental types --- */
extern DskXmlBindingType dsk_xml_binding_type_int;
extern DskXmlBindingType dsk_xml_binding_type_uint;
extern DskXmlBindingType dsk_xml_binding_type_float;
extern DskXmlBindingType dsk_xml_binding_type_double;
extern DskXmlBindingType dsk_xml_binding_type_string;

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
dsk_boolean dsk_xml_binding_union_parse  (DskXmlBindingType *type,
		                          DskXml            *to_parse,
		                          void              *out,
		                          DskError         **error);
DskXml  *   dsk_xml_binding_union_to_xml (DskXmlBindingType *type,
		                          const char        *data,
		                          DskError         **error);
void        dsk_xml_binding_union_clear  (DskXmlBindingType *type,
		                          void              *out);
