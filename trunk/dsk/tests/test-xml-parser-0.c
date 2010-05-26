#include "../dsk.h"

static DskXml *
load_valid_xml (const char *str,
                DskXmlParserConfig *config)
{
  DskXmlParser *parser;
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
  while (*patterns_at != 0)
    {
      unsigned *start_pattern = patterns_at;
      DskXmlParser *parser = dsk_xml_parser_new (config, NULL);
      const char *str_at = str;
      unsigned rem = str_length;
      dsk_assert (parser);
      while (rem != 0)
        {
          ...
        }
      for (patterns_at = start_pattern; *patterns_at; patterns_at++)
        {
          ...
        }
    }
  ...
}


static void
test_simple_0 (void)
{
  ...
}
