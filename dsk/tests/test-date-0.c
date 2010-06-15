#include <stdio.h>
#include "../dsk.h"

/* Test dsk_date_parse() and dsk_date_to_unixtime() */
static void
test_dates_0 (void)
{
  static const char *dates[] =
  {
    "Sunday, 06-Nov-94 08:49:37 GMT", /* RFC 850/1036 dates */
    "Sun, 06 Nov 1994 08:49:37 GMT", /* RFC 822/1123 dates */
    "Sun Nov  6 08:49:37 1994", /* ANSI C dates */
    "2009-02-12 T 14:32:61.1+01:00" /* see ISO 8601/RFC 3339 */
  };
  unsigned i;
 
  for (i = 0; i < DSK_N_ELEMENTS (dates); i++)
    {
      DskDate date;
      DskError *error = NULL;
      if (!dsk_date_parse (dates[i], NULL, &date, &error))
        dsk_die ("error parsing '%s': %s", dates[i], error->message);
      dsk_assert (date.year == 1994);
      dsk_assert (date.month == 11);
      dsk_assert (date.day == 6);
      dsk_assert (date.hour == 8);
      dsk_assert (date.minute == 49);
      dsk_assert (date.second == 37);
      dsk_assert (date.zone_offset == 0);
      dsk_assert (dsk_date_to_unixtime (&date) == 784111777);
    }
}
static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple handling", test_dates_0 },
};
int main(int argc, char **argv)
{
  unsigned i;
  dsk_cmdline_init ("test date handling",
                    "Test various DSK date parsing/printing and manipulation",
                    NULL, 0);
  //dsk_cmdline_add_boolean ("verbose", "extra logging", NULL, 0,
                           //&cmdline_verbose);
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
