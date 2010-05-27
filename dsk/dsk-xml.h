typedef struct _DskXmlFilename DskXmlFilename;
typedef struct _DskXml DskXml;

typedef enum
{
  DSK_XML_ELEMENT,
  DSK_XML_TEXT,
  DSK_XML_COMMENT
} DskXmlType;

struct _DskXmlFilename
{
  unsigned ref_count;
  char *filename;
};

struct _DskXml
{
  /* all fields public and read-only */

  unsigned line_no;
  DskXmlFilename *filename;
  DskXmlType type;
  unsigned ref_count;
  char *str;		/* name for ELEMENT, text for COMMENT and TEXT */
  char **attrs;		/* key-value pairs */
  unsigned n_children;
  DskXml **children;
};

DskXml *dsk_xml_ref   (DskXml *xml);
void    dsk_xml_unref (DskXml *xml);

/* --- TODO: add wad of constructors --- */
DskXml *dsk_xml_text_new_len (unsigned len,
                              const char *data);



/* --- comments (not recommended much) --- */
DskXml *dsk_xml_comment_new_len (unsigned len,
                                 const char *text);

/* FOR INTERNAL USE ONLY: create an xml node from a packed set of
   attributes, and a set of children, which we will take ownership of.
   We will do text-node compacting.
 */
DskXml *_dsk_xml_new_elt_parse (unsigned n_attrs,
                                unsigned name_kv_space,
                                const char *name_and_attrs,
                                unsigned n_children,
                                DskXml **children,
                                dsk_boolean condense_text_nodes);
void _dsk_xml_set_position (DskXml *xml,
                            DskXmlFilename *filename,
                            unsigned line_no);
