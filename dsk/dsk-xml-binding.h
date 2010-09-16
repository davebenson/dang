
typedef struct _DskXmlBindingType DskXmlBindingType;
typedef struct _DskXmlBinding DskXmlBinding;
typedef struct _DskXmlBindingStructMember DskXmlBindingStructMember;
typedef struct _DskXmlBindingTypeStruct DskXmlBindingTypeStruct;
typedef struct _DskXmlBindingUnionCase DskXmlBindingUnionCase;
typedef struct _DskXmlBindingTypeUnion DskXmlBindingTypeUnion;
typedef struct _DskXmlBindingNamespace DskXmlBindingNamespace;

typedef enum
{
  DSK_XML_BINDING_REQUIRED,
  DSK_XML_BINDING_OPTIONAL,
  DSK_XML_BINDING_REPEATED,
  DSK_XML_BINDING_REQUIRED_REPEATED
} DskXmlBindingQuantity;


struct _DskXmlBindingType
{
  unsigned is_fundamental : 1;
  unsigned is_static : 1;
  unsigned is_struct : 1;
  unsigned is_union : 1;

  unsigned sizeof_instance;
  unsigned alignof_instance;
  DskXmlBindingNamespace *ns;
  char *name;

  /* virtual functions */
  dsk_boolean (*parse)(DskXmlBindingType *type,
                       DskXml            *to_parse,
		       void              *out,
		       DskError         **error);
  DskXml   *  (*to_xml)(DskXmlBindingType *type,
                        const void        *data,
		        DskError         **error);
  void        (*clear) (DskXmlBindingType *type,
		        void              *out);
};


struct _DskXmlBindingNamespace
{
  dsk_boolean is_static;
  char *name;
  unsigned n_types;
  DskXmlBindingType **types;
};


DskXmlBinding *dsk_xml_binding_new (void);
void           dsk_xml_binding_add_searchpath (DskXmlBinding *binding,
                                               const char    *path,
                                               const char    *ns_separator);
DskXmlBindingNamespace*
               dsk_xml_binding_get_ns         (DskXmlBinding *binding,
                                               const char    *name,
                                               DskError     **error);
DskXmlBindingNamespace*
               dsk_xml_binding_try_ns         (DskXmlBinding *binding,
                                               const char    *name);

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
  unsigned *members_sorted_by_name;
  unsigned sizeof_struct;
};
DskXmlBindingTypeStruct *dsk_xml_binding_struct_new (DskXmlBindingNamespace *ns,
                                                 const char        *struct_name,
                                                 unsigned           n_members,
                                   const DskXmlBindingStructMember *members);

int dsk_xml_binding_type_struct_lookup_member (DskXmlBindingTypeStruct *type,
                                               const char              *name);

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
