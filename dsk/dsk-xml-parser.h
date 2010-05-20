

typedef struct _DskXmlParserNamespaceConfig DskXmlParserNamespaceConfig;
typedef struct _DskXmlParserConfig DskXmlParserConfig;
typedef struct _DskXmlParser DskXmlParser;


/* --- xml parsing --- */

struct _DskXmlParserNamespaceConfig
{
  char *url;
  char *prefix;
};

typedef enum
{
  DSK_XML_PARSER_IGNORE_NS           = (1<<0),
  DSK_XML_PARSER_SUPPRESS_WHITESPACE = (1<<1),
  DSK_XML_PARSER_SUPPRESS_COMMENTS   = (1<<2)
} DskXmlParserFlags;

DskXmlParserConfig *
dsk_xml_parser_config_new (DskXmlParserFlags flags,
			   unsigned          n_ns,
			   const DskXmlParserNamespaceConfig *ns,
			   unsigned          n_xpaths,
			   char             *xpaths);
void
dsk_xml_parser_config_destroy (DskXmlParserConfig *config);


DskXmlParser *dsk_xml_parser_new (DskXmlParserConfig *config,
                                  const char         *display_filename);
DskXml       *dsk_xml_parser_pop (DskXmlParser       *parser,
                                  unsigned           *xpath_index_out);
dsk_boolean   dsk_xml_parser_feed(DskXmlParser       *parser,
                                  unsigned            len,
                                  char               *data,
                                  DskError          **error_out);
void          dsk_xml_parser_free(DskXmlParser       *parser);

