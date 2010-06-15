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
    "1994-11-06 T 08:49:37.1+00:00" /* see ISO 8601/RFC 3339 */
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
static void
test_timezones (void)
{
  static const struct {
    const char *str;
    unsigned length;
    int minutes;
  } tz_tests[] = {
    { "GMT  ", 3, 0 },
    { "UTC  ", 3, 0 },
    { "UT   ", 2, 0 },
    { "PST ", 3, -8*60  },
    { "PDT ", 3, -7*60  },
    { "MST ", 3, -7*60  },
    { "MDT ", 3, -6*60  },
    { "CST ", 3, -6*60  },
    { "CDT ", 3, -5*60  },
    { "EST ", 3, -5*60  },
    { "EDT ", 3, -4*60  },
    { "Z ",   1, 0 },
    { "GmT  ", 3, 0 },
    { "UtC  ", 3, 0 },
    { "Ut   ", 2, 0 },
    { "PsT ", 3, -8*60  },
    { "PdT ", 3, -7*60  },
    { "MsT ", 3, -7*60  },
    { "MdT ", 3, -6*60  },
    { "CsT ", 3, -6*60  },
    { "CdT ", 3, -5*60  },
    { "EsT ", 3, -5*60  },
    { "EdT ", 3, -4*60  },
    { "Z ",   1, 0 },
    { "Gmt  ", 3, 0 },
    { "Utc  ", 3, 0 },
    { "Ut   ", 2, 0 },
    { "Pst ", 3, -8*60  },
    { "Pdt ", 3, -7*60  },
    { "Mst ", 3, -7*60  },
    { "Mdt ", 3, -6*60  },
    { "Cst ", 3, -6*60  },
    { "Cdt ", 3, -5*60  },
    { "Est ", 3, -5*60  },
    { "Edt ", 3, -4*60  },
    { "gmt  ", 3, 0 },
    { "utc  ", 3, 0 },
    { "ut   ", 2, 0 },
    { "pst ", 3, -8*60  },
    { "pdt ", 3, -7*60  },
    { "mst ", 3, -7*60  },
    { "mdt ", 3, -6*60  },
    { "cst ", 3, -6*60  },
    { "cdt ", 3, -5*60  },
    { "est ", 3, -5*60  },
    { "edt ", 3, -4*60  },
    { "z ",   1, 0 },
    { "+0130 ", 5, 90 },
    { "-0130 ", 5, -90 },
    { "+01:30 ", 6, 90 },
    { "-01:30 ", 6, -90 },
  };
  unsigned i;
  for (i = 0; i < DSK_N_ELEMENTS (tz_tests); i++)
    {
      char *end;
      int tz;
      if (!dsk_date_parse_timezone (tz_tests[i].str, &end, &tz))
        dsk_die ("error parsing timezone '%s'", tz_tests[i].str);
      dsk_assert (tz_tests[i].length == (unsigned)(end - tz_tests[i].str));
      dsk_assert (tz == tz_tests[i].minutes);
    }
}
static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple parsing", test_dates_0 },
  { "timezone parsing", test_timezones },
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
