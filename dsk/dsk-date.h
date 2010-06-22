
/* A Gregorian Date class.
 *
 * Wikipedia has a list of other calendar systems, which is neat:
 *      http://en.wikipedia.org/wiki/List_of_calendars
 * However, none of these is used in any protocols,
 * so we do not implement them at all.
 */


typedef struct _DskDate DskDate;
struct _DskDate
{
  unsigned year;	/* the year (like tm_year+1900) */
  unsigned month;	/* the month (1 through 12, tm_mon+1) */
  unsigned day;		/* the day of month (1..31) */
  unsigned hour;	/* 0..23 */
  unsigned minute;	/* 0..59 */
  unsigned second;	/* 0..60 (60 is required to support leap-seconds) */
  int      zone_offset; /* in minutes-- negative is west */
};

/* Parse either RFC 822/1123 dates (Sun, 06 Nov 1994 08:49:37 GMT)
 * or RFC 850/1036 dates (Sunday, 06-Nov-94 08:49:37 GMT)
 * or ANSI C dates (Sun Nov  6 08:49:37 1994)
 * or ISO 8601 dates (2009-02-12 T 14:32:61.1+01:00) (see RFC 3339)
 */
dsk_boolean dsk_date_parse   (const char *str,
                              char      **end,
                              DskDate    *out,
                              DskError  **error);

#define DSK_DATE_MAX_LENGTH 64
void        dsk_date_print_rfc822 (DskDate *date,
                                   char    *buf);
void        dsk_date_print_rfc850 (DskDate *date, /* unimplemented */
                                   char    *buf);
void        dsk_date_print_iso8601 (DskDate *date, /* unimplemented */
                                   char    *buf);

/* 'unixtime' here is seconds since epoch.
   If the date is before the epoch (Jan 1, 1970 00:00 GMT),
   then it is negative. */
/* Behavior of dsk_date_to_unixtime() is undefined if any
   of the fields are out-of-bounds.
   Behavior of dsk_unixtime_to_date() is defined for any time
   whose year fits in an unsigned integer. (therefore, no B.C. dates)
 */
/* dsk_unixtime_to_date() always sets the date_out->zone_offset to 0 (ie GMT) */
dsk_time_t  dsk_date_to_unixtime (DskDate *date);
void        dsk_unixtime_to_date (dsk_time_t unixtime,
                                  DskDate *date_out);

/* we recognise: UT, UTC, GMT; EST EDT CST CDT MST MDT PST PDT [A-Z]
   and the numeric formats: +#### and -#### and +##:## and -##:##

   The return value is the number of minutes you must add to the time
   to get the time in Greenwich.  */
dsk_boolean dsk_date_parse_timezone (const char *at,
                                     char **end,
				     int *zone_offset_out);

int dsk_date_get_days_since_epoch (DskDate *);

#if 0
/* UNIMPLEMENTED:  this is the API to deal with localtime
   fully..  To use it requires being able to parse zoneinfo files.
   Which is too much work for a feature we basically don't need. */
DskTimezone *dsk_timezone_get  (const char *name,
                                DskError  **error);
dsk_boolean dsk_timezone_query (DskTimezone *timezone,
                                dsk_time_t   unixtime,
                                int         *minutes_offset_out,
                                const char **timezone_name_out,
                                DskError   **error);
#endif
