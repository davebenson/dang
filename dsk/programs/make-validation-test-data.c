#include "../dsk.h"

int main(int argc, char **argv)
{
  dsk_cmdline_init ("standard xml conformance test",
                    "Run some standard conformance test on our XML parser",
                    NULL, 0);
  dsk_cmdline_add_boolean ("print-error", "print error messages for bad-response tests", NULL, 0,
                           &print_errors);
  dsk_cmdline_add_boolean ("verbose", "print all test statuses", NULL, 0,
                           &verbose);
  dsk_cmdline_process_args (&argc, &argv);


  config = dsk_xml_parser_config_new_simple (0, "TESTCASES/TEST");
  parser = dsk_xml_parser_new (config);

  for (;;)
    {
      /* read from file */
      uint8_t buf[4096];
      int nread = fread (buf, 1, sizeof (buf), fp);
      if (nread < 0)
        dsk_die ("error reading from stdin");
      if (nread == 0)
        break;

      /* parse */
      if (!dsk_xml_parser_feed (parser, nread, buf, &error))
        dsk_die ("error parsing xml: %s", error->message);

      /* handle XML nodes */
      while ((xml=dsk_xml_parser_pop (parser)) != NULL)
        {
          ...
          dsk_xml_unref (xml);
        }
    }
  return 0;
}
