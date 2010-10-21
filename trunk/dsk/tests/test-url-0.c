#include <string.h>
#include <stdio.h>
#include "../dsk.h"

static dsk_boolean cmdline_verbose = DSK_FALSE;

#define SCAN(url, scan)                                                \
  do{                                                                  \
    DskError *error = NULL;                                            \
    if (cmdline_verbose)                                               \
      dsk_warning ("scanning '%s' at %s:%u", url, __FILE__, __LINE__); \
    if (!dsk_url_scan (url, scan, &error))                             \
      dsk_die ("error scanning url %s at %s:%u: %s",                   \
               url, __FILE__, __LINE__, error->message);               \
  }while(0)

static void
test_simple_http (void)
{
  DskUrlScanned scan;
  const char *url;

  url = "http://foo.com/bar";
  SCAN (url, &scan);
  dsk_assert (scan.scheme_start == url);
  dsk_assert (scan.scheme_end == url + 4);
  dsk_assert (scan.username_start == NULL);
  dsk_assert (scan.username_end == NULL);
  dsk_assert (scan.password_start == NULL);
  dsk_assert (scan.password_end == NULL);
  dsk_assert (scan.host_start == url + 7);
  dsk_assert (scan.host_end == url + 14);
  dsk_assert (scan.port_start == NULL);
  dsk_assert (scan.port_end == NULL);
  dsk_assert (scan.port == 0);
  dsk_assert (scan.path_start == scan.host_end);
  dsk_assert (scan.path_end == url + 18);
  dsk_assert (scan.query_start == NULL);
  dsk_assert (scan.query_end == NULL);
  dsk_assert (scan.fragment_start == NULL);
  dsk_assert (scan.fragment_end == NULL);
}

static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple HTTP", test_simple_http },
  { "simple FTP", test_simple_ftp },
  { "HTTP with port", test_http_port },
  { "HTTP relative hostless", test_http_relative },
  { "HTTP absolute hostless", test_http_absolute },
  { "username and password", test_userpass },
  { "username", test_userpass },
  { "query", test_query },
  { "fragment", test_fragment },
  { "query and fragment", test_query_and_frag },
};
int main(int argc, char **argv)
{
  unsigned i;
  dsk_cmdline_init ("test URL parsing",
                    "Test URL scanning for a variety of URLs",
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
