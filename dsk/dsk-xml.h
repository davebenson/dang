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

