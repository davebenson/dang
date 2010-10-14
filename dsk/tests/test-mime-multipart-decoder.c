#include <stdio.h>
#include <string.h>
#include "../dsk.h"

static dsk_boolean cmdline_verbose = DSK_FALSE;

static void
test_mime_multipart_decoder_simple (void)
{
  unsigned n_cgi_vars;
  DskCgiVar *cgi_vars;
  unsigned iter;

  /* Example from RFC 2046, Section 5.1.1.  Page 21. */
  static const uint8_t mime_body[] =
     "This is the preamble.  It is to be ignored, though it\r\n"
     "is a handy place for composition agents to include an\r\n"
     "explanatory note to non-MIME conformant readers.\r\n"
     "\r\n"
     "--simple boundary\r\n"
     "\r\n"
     "This is implicitly typed plain US-ASCII text.\r\n"
     "It does NOT end with a linebreak.\r\n"
     "--simple boundary\r\n"
     "Content-type: text/plain; charset=us-ascii\r\n"
     "\r\n"
     "This is explicitly typed plain US-ASCII text.\r\n"
     "It DOES end with a linebreak.\r\n"
     "\r\n"
     "--simple boundary--\r\n"
     ;
  const char *content_type_kv_pairs[] =
    {
      "boundary",
      "simple-boundary",
      NULL
    };
  
  /* Run test twice:  first feed the data as one blob,
     then feed it byte-by-byte. */
  for (iter = 0; iter < 2; iter++)
    {
      DskError *error = NULL;
      const char *tmp_txt;
      DskMimeMultipartDecoder *decoder = dsk_mime_multipart_decoder_new ((char**)content_type_kv_pairs, &error);
      if (decoder == NULL)
        dsk_die ("dsk_mime_multipart_decoder_new failed: %s", error->message);
      switch (iter)
        {
        case 0:
          {
          /* feed the data all at once */
            unsigned len = strlen ((char*)mime_body);
            if (!dsk_mime_multipart_decoder_feed (decoder,
                                                  len, mime_body,
                                                  &n_cgi_vars, &error))
              dsk_die ("error calling dsk_mime_multipart_decoder_feed(%u): %s",
                       len, error->message);
          }
          break;
        case 1:
          /* feed the data a byte at a time */
          {
            unsigned i;
            for (i = 0; mime_body[i] != '\0'; i++)
              if (!dsk_mime_multipart_decoder_feed (decoder, 1, mime_body + i,
                                                    &n_cgi_vars, &error))
                dsk_die ("error calling dsk_mime_multipart_decoder_feed(1): %s",
                         error->message);
          }
          break;
        }
      if (!dsk_mime_multipart_decoder_done (decoder, &n_cgi_vars, &error))
        dsk_die ("error calling dsk_mime_multipart_decoder_done: %s", error->message);

      dsk_assert (n_cgi_vars == 2);

      tmp_txt = "This is implicitly typed plain US-ASCII text.\r\n"
                "It does NOT end with a linebreak.";
      dsk_assert (cgi_vars[0].value_length == strlen (tmp_txt));
      dsk_assert (memcmp (tmp_txt, cgi_vars[0].value, strlen (tmp_txt)) == 0);

      tmp_txt = "This is explicitly typed plain US-ASCII text.\r\n"
                "It DOES end with a linebreak.\r\n";
      dsk_assert (cgi_vars[1].value_length == strlen (tmp_txt));
      dsk_assert (memcmp (tmp_txt, cgi_vars[1].value, strlen (tmp_txt)) == 0);
      dsk_assert (strcmp (cgi_vars[1].content_type, "text/plain") == 0);
    }
}

static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple example decoding", test_mime_multipart_decoder_simple },
};

int main(int argc, char **argv)
{
  unsigned i;

  dsk_cmdline_init ("test mime multipart decoder",
                    "Test the MIME Multipart Decoder",
                    NULL, 0);
  dsk_cmdline_add_boolean ("verbose", "extra logging", NULL, 0,
                           &cmdline_verbose);
  dsk_cmdline_process_args (&argc, &argv);

  for (i = 0; i < DSK_N_ELEMENTS (tests); i++)
    {
      fprintf (stderr, "Test: %s... ", tests[i].name);
      tests[i].test ();
      fprintf (stderr, " done.\n");
    }
  dsk_cleanup ();
  return 0;
}
