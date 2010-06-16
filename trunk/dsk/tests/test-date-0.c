#include <string.h>
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

static void
test_date_to_timet (void)
{
  static struct {
    DskDate date;
    int64_t unixtime;
  } d_tests[] = {
        { { 2010, 6, 15, 18, 37, 53, 0 }, 1276627073 },
        { { 1971, 11, 27, 00, 48, 55, 0 }, 60050935 },
        { { 1999, 10, 17, 19, 20, 34, 0 }, 940188034 },
        { { 2006, 10, 19, 17, 21, 57, 0 }, 1161278517 },
        { { 1979, 7, 23, 18, 23, 03, 0 }, 301602183 },
        { { 1971, 6, 23, 05, 12, 59, 0 }, 46501979 },
        { { 1973, 9, 1, 16, 59, 19, 0 }, 115750759 },
        { { 1996, 7, 14, 17, 04, 43, 0 }, 837363883 },
        { { 1986, 9, 9, 21, 24, 38, 0 }, 526685078 },
        { { 1975, 4, 9, 15, 21, 51, 0 }, 166288911 },
        { { 1987, 11, 10, 10, 13, 35, 0 }, 563537615 },
        { { 1999, 3, 5, 07, 36, 35, 0 }, 920619395 },
        { { 1981, 1, 4, 01, 59, 23, 0 }, 347421563 },
        { { 1983, 9, 18, 22, 11, 18, 0 }, 432771078 },
        { { 1989, 6, 17, 06, 15, 31, 0 }, 614067331 },
        { { 2007, 12, 24, 05, 38, 21, 0 }, 1198474701 },
        { { 2007, 2, 5, 15, 45, 27, 0 }, 1170690327 },
        { { 1997, 1, 5, 8, 18, 29, 0 }, 852452309 },
        { { 2009, 10, 30, 06, 8, 07, 0 }, 1256882887 },
        { { 2008, 1, 15, 00, 12, 06, 0 }, 1200355926 },
        { { 1987, 10, 22, 04, 58, 10, 0 }, 561877090 },
        { { 1998, 7, 21, 9, 20, 18, 0 }, 901012818 },
        { { 1982, 10, 13, 06, 40, 25, 0 }, 403339225 },
        { { 2000, 3, 31, 20, 18, 12, 0 }, 954533892 },
        { { 2006, 4, 5, 12, 02, 25, 0 }, 1144238545 },
        { { 1987, 12, 15, 22, 8, 8, 0 }, 566604488 },
        { { 2009, 9, 24, 15, 57, 31, 0 }, 1253807851 },
        { { 1978, 4, 13, 02, 20, 28, 0 }, 261282028 },
        { { 1996, 8, 27, 13, 58, 42, 0 }, 841154322 },
        { { 1979, 4, 13, 02, 28, 03, 0 }, 292818483 },
        { { 1976, 8, 8, 11, 58, 24, 0 }, 208353504 },
        { { 1979, 7, 29, 23, 36, 40, 0 }, 302139400 },
        { { 1979, 4, 18, 22, 39, 17, 0 }, 293323157 },
        { { 2008, 11, 8, 20, 45, 13, 0 }, 1226177113 },
        { { 1973, 3, 9, 21, 52, 15, 0 }, 100561935 },
        { { 1981, 8, 3, 13, 02, 21, 0 }, 365691741 },
        { { 2009, 6, 20, 21, 49, 15, 0 }, 1245534555 },
        { { 2002, 4, 11, 04, 04, 39, 0 }, 1018497879 },
        { { 2011, 2, 6, 05, 49, 02, 0 }, 1296971342 },
        { { 2002, 3, 25, 20, 36, 25, 0 }, 1017088585 },
        { { 1985, 7, 1, 10, 25, 52, 0 }, 489061552 },
        { { 2007, 2, 1, 06, 35, 43, 0 }, 1170311743 },
        { { 1983, 6, 4, 9, 00, 19, 0 }, 423565219 },
        { { 1999, 9, 4, 12, 41, 45, 0 }, 936448905 },
        { { 1995, 3, 7, 14, 32, 51, 0 }, 794586771 },
        { { 2011, 2, 1, 05, 19, 48, 0 }, 1296537588 },
        { { 1992, 7, 6, 9, 43, 03, 0 }, 710415783 },
        { { 1982, 1, 8, 21, 57, 57, 0 }, 379375077 },
        { { 1974, 5, 20, 21, 54, 23, 0 }, 138318863 },
        { { 1988, 11, 11, 5, 51, 18, 0 }, 595230678 },
        { { 1972, 9, 16, 22, 50, 54, 0 }, 85531854 },
        { { 2004, 7, 5, 21, 11, 01, 0 }, 1089061861 },
        { { 2002, 8, 20, 04, 27, 21, 0 }, 1029817641 },
        { { 2009, 8, 27, 21, 10, 04, 0 }, 1251407404 },
        { { 2007, 1, 20, 8, 41, 16, 0 }, 1169282476 },
        { { 1976, 3, 19, 11, 18, 20, 0 }, 196082300 },
        { { 1992, 1, 22, 10, 06, 20, 0 }, 696074780 },
        { { 1972, 9, 1, 23, 07, 00, 0 }, 84236820 },
        { { 1991, 10, 6, 17, 24, 46, 0 }, 686769886 },
        { { 1996, 12, 8, 12, 17, 29, 0 }, 850047449 },
        { { 2010, 4, 28, 11, 59, 39, 0 }, 1272455979 },
        { { 2003, 12, 25, 19, 05, 23, 0 }, 1072379123 },
        { { 1976, 9, 29, 14, 56, 56, 0 }, 212857016 },
        { { 1980, 3, 18, 11, 05, 53, 0 }, 322225553 },
        { { 1987, 2, 9, 13, 29, 55, 0 }, 539875795 },
        { { 1975, 5, 29, 13, 33, 33, 0 }, 170602413 },
        { { 1996, 2, 13, 00, 52, 30, 0 }, 824172750 },
        { { 1980, 4, 10, 03, 23, 50, 0 }, 324185030 },
        { { 1993, 2, 12, 16, 58, 37, 0 }, 729536317 },
        { { 1994, 3, 21, 10, 9, 11, 0 }, 764244551 },
        { { 1974, 12, 6, 20, 00, 24, 0 }, 155592024 },
        { { 1985, 8, 15, 17, 03, 47, 0 }, 492973427 },
        { { 1994, 7, 24, 02, 14, 38, 0 }, 775016078 },
        { { 1970, 11, 2, 11, 02, 04, 0 }, 26391724 },
        { { 2003, 1, 8, 04, 22, 06, 0 }, 1041999726 },
        { { 1981, 11, 27, 9, 50, 44, 0 }, 375702644 },
        { { 1995, 11, 28, 11, 52, 35, 0 }, 817559555 },
        { { 1989, 11, 15, 21, 06, 42, 0 }, 627167202 },
        { { 2007, 8, 7, 13, 01, 04, 0 }, 1186491664 },
        { { 1985, 10, 3, 10, 27, 26, 0 }, 497183246 },
        { { 1988, 8, 19, 17, 45, 46, 0 }, 588015946 },
        { { 2000, 3, 16, 02, 14, 12, 0 }, 953172852 },
        { { 2001, 7, 21, 21, 07, 52, 0 }, 995749672 },
        { { 1987, 3, 29, 8, 10, 14, 0 }, 544003814 },
        { { 1994, 9, 16, 20, 22, 05, 0 }, 779746925 },
        { { 1975, 12, 9, 16, 34, 14, 0 }, 187374854 },
        { { 1995, 3, 8, 13, 53, 17, 0 }, 794670797 },
        { { 1990, 5, 4, 9, 42, 27, 0 }, 641814147 },
        { { 2009, 10, 14, 10, 54, 43, 0 }, 1255517683 },
        { { 2006, 10, 23, 13, 10, 31, 0 }, 1161609031 },
        { { 1970, 2, 7, 18, 46, 35, 0 }, 3264395 },
        { { 2005, 8, 5, 9, 42, 15, 0 }, 1123234935 },
        { { 1997, 7, 30, 12, 46, 17, 0 }, 870266777 },
        { { 1997, 5, 7, 9, 36, 9, 0 }, 862997769 },
        { { 1972, 6, 15, 23, 55, 15, 0 }, 77500515 },
        { { 2001, 1, 15, 5, 7, 59, 0 }, 979535279 },
        { { 1974, 8, 16, 14, 10, 35, 0 }, 145894235 },
        { { 1981, 8, 8, 0, 52, 11, 0 }, 366079931 },
        { { 1978, 9, 26, 10, 50, 33, 0 }, 275655033 },
        { { 2004, 10, 3, 13, 19, 34, 0 }, 1096809574 },
        { { 2006, 12, 22, 15, 39, 59, 0 }, 1166801999 },
  };
  unsigned i;
  for (i = 0; i < DSK_N_ELEMENTS (d_tests); i++)
    {
      dsk_time_t t = dsk_date_to_unixtime (&d_tests[i].date);
      DskDate d;
      dsk_assert (t == d_tests[i].unixtime);
      dsk_unixtime_to_date (t, &d);
      dsk_assert (d_tests[i].date.year == d.year);
      dsk_assert (d_tests[i].date.month == d.month);
      dsk_assert (d_tests[i].date.day == d.day);
      dsk_assert (d_tests[i].date.hour == d.hour);
      dsk_assert (d_tests[i].date.minute == d.minute);
      dsk_assert (d_tests[i].date.second == d.second);
    }
}

static void
test_printing (void)
{
  DskDate date = { 2006, 12, 22, 15, 39, 59, 0 };
  char buf[DSK_DATE_MAX_LENGTH];
  dsk_date_print_rfc822 (&date, buf);
  dsk_assert (strcmp (buf, "Fri, 22 Dec 2006 15:39:59 +0000") == 0);
#if 0
  dsk_date_print_rfc850 (&date, buf);
  dsk_assert (strcmp (buf, "Friday, 22-Dec-2006 15:39:59 +0000") == 0);
  dsk_date_print_iso8601 (&date, buf);
  dsk_assert (strcmp (buf, "2006-12-22 T 15:39:59+00:00") == 0);
#endif
}


static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple parsing", test_dates_0 },
  { "timezone parsing", test_timezones },
  { "date <-> unixtime", test_date_to_timet },
  { "printing", test_printing },
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
