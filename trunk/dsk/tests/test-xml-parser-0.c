#include <string.h>
#include "../dsk.h"
static dsk_boolean
xml_equal (DskXml *a, DskXml *b)
{
  if (a->type != b->type)
    return DSK_FALSE;
  if (strcmp (a->str, b->str) != 0)
    return DSK_FALSE;
  if (a->type == DSK_XML_ELEMENT)
    {
      unsigned i;
      for (i = 0; ; i++)
        {
          if (a->attrs[i] == NULL && b->attrs[i] == NULL)
            break;
          if (a->attrs[i] == NULL || b->attrs[i] == NULL)
            return DSK_FALSE;
          if (strcmp (a->attrs[i], b->attrs[i]) != 0)
            return DSK_FALSE;
        }
      if (a->n_children != b->n_children)
        return DSK_FALSE;
      for (i = 0; i < a->n_children; i++)
        if (!xml_equal (a->children[i], b->children[i]))
          return DSK_FALSE;
    }
  return DSK_TRUE;
}

static DskXml *
load_valid_xml (const char *str,
                DskXmlParserConfig *config)
{
  unsigned str_length = strlen (str);

  /* patterns: an array of 0-terminated arrays signifying
   * what size chunks to feed the data to parser in.
   */
  static unsigned patterns[] = {
    1,0,               /* load byte-by-byte */
    1000000000,0,      /* load the entire thing in one go */
    1,13,0,
    1,10,0,
    1,100,0,
    1,1000,0,
    1,10000,0,
    0
  };
  unsigned *patterns_at = patterns;
  DskXml *rv = NULL;
  while (*patterns_at != 0)
    {
      unsigned *start_pattern = patterns_at;
      DskXmlParser *parser = dsk_xml_parser_new (config, NULL);
      const char *str_at = str;
      unsigned rem = str_length;
      dsk_assert (parser);
      while (rem != 0)
        {
          for (patterns_at = start_pattern; *patterns_at && rem != 0; patterns_at++)
            {
              unsigned use = rem < *patterns_at ? rem : *patterns_at;
              DskError *error = NULL;
              if (!dsk_xml_parser_feed (parser, use, str_at, &error))
                dsk_die ("error feeding xml to parser: %s", error->message);
              rem -= use;
              str_at += use;
            }
        }
      DskXml *xml = dsk_xml_parser_pop (parser, 0);
      dsk_assert (xml != NULL);
      dsk_assert (dsk_xml_parser_pop (parser, 0) == NULL);
      if (start_pattern == patterns)
        rv = xml;
      else
        {
          if (!xml_equal (rv, xml))
            dsk_die ("feeding different ways led to different xml");
          dsk_xml_unref (xml);
        }
    }
  return rv;
}


static void
test_simple_0 (void)
{
  char *empty = "*";
  DskError *error = NULL;
  DskXmlParserConfig *config = dsk_xml_parser_config_new (0, 0, NULL,
                                                          1, &empty,
                                                          &error);
  dsk_assert (error != NULL);
  DskXml *xml = load_valid_xml ("<abc>tmp</abc>", config);
  dsk_assert (xml->type == DSK_XML_ELEMENT);
  dsk_assert (strcmp (xml->str, "abc") == 0);
  dsk_assert (xml->n_children == 1);
  dsk_assert (xml->children[0]->type == DSK_XML_TEXT);
  dsk_assert (strcmp (xml->children[0]->str, "tmp") == 0);
  dsk_xml_unref (xml);
  dsk_xml_parser_config_destroy (config);
}

int main()
{
  test_simple_0 ();
  return 0;
}
