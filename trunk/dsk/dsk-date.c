
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

static dsk_boolean
ascii_is_alpha (char c)
{
  return ('a' <= c && c <= 'z')
      || ('A' <= c && c <= 'Z');
}

static dsk_boolean
parse_generic (const char *format,
               const char *str,
               char      **end,
               DskDate    *out,
               DskError  **error)
{
  unsigned code;
  while (*format)
    {
      if (*format == ' ')
        {
          while (ascii_is_space (*at))
            at++;
          format++;
          continue;
        }
      if (*format == '-' || *format == ':' || *format == ',')
        {
          if (*at != *format)
            {
              dsk_set_error (error, "expected '%c' in date (offset %u)", *format, (unsigned)(at-str));
              return DSK_FALSE;
            }
          at++;
          format++;
        }
      dsk_assert (*format == '%');
      switch (format[1])
        {
        case 'A':
          while (ascii_is_alpha (at))
            at++;
          format += 2;
          break;
        case 'Y':
          if (!ascii_is_digit (at[0]) 
           || !ascii_is_digit (at[1]) 
           || !ascii_is_digit (at[2]) 
           || !ascii_is_digit (at[3]))
            {
              dsk_set_error (error, "expected 4-digit year");
              return DSK_FALSE;
            }
          out->year = (unsigned)(at[0] - '0') * 1000
                    + (unsigned)(at[1] - '0') * 100
                    + (unsigned)(at[2] - '0') * 10
                    + (unsigned)(at[3] - '0');
          at += 4;
          format += 2;
          break;
        case 'y':
          if (!ascii_is_digit (at[0]) 
           || !ascii_is_digit (at[1]) )
            {
              dsk_set_error (error, "expected year");
              return DSK_FALSE;
            }
          if (ascii_is_digit (at[2]))
            {
              if (!ascii_is_digit (at[3]))
                {
                  dsk_set_error (error, "3-digit year?");
                  return DSK_FALSE;
                }
              out->year = (unsigned)(at[0] - '0') * 1000
                        + (unsigned)(at[1] - '0') * 100
                        + (unsigned)(at[2] - '0') * 10
                        + (unsigned)(at[3] - '0');
              at += 4;
            }
          else
            {
              out->year = (unsigned)(at[0] - '0') * 10
                        + (unsigned)(at[1] - '0');
              if (out->year < 70)
                out->year += 2000;
              else
                out->year += 1900;
              at += 2;
            }
          format += 2;
          break;
        case 'd':
          if (!ascii_is_digit (at[0]) || !ascii_is_digit (at[1]))
            {
              msg = "expected two digit day-of-month";
              goto error;
            }
          out->day = (at[0] - '0') * 10 + (at[1] - '0');
          at += 2;
          format += 2;
          break;
        case 'b':
          /* accept 3-letter month abbrev */
          if (!ascii_is_alpha (at[0])
           || !ascii_is_alpha (at[1])
           || !ascii_is_alpha (at[2]))
            {
              dsk_set_error (error, "expected three letter month abbrev (at offset %u)",
                             (unsigned)(at-str));
              return DSK_FALSE;
            }
          code = ((unsigned int)ascii_lower_alpha (at[0]) << 16)
               | ((unsigned int)ascii_lower_alpha (at[1]) << 8)
               | ((unsigned int)ascii_lower_alpha (at[2]) << 0);
          out->month = get_month_from_code (code);
          if (out->month == 0)
            {
              dsk_set_error (error, "invalid month name (at offset %u)",
                             (unsigned)(at-str));
              return DSK_FALSE;
            }
          break;
        case 'm':
          /* accept 2-digit month number */
          if (!ascii_is_digit (at[0]) 
           || !ascii_is_digit (at[1]))
            {
              dsk_set_error (error, "expected two digit month number (at offset %u)",
                             (unsigned)(at-str));
              return DSK_FALSE;
            }
          out->month = (at[0] - '0') * 10 + (at[1] - '0');
          at += 2;
          format += 2;
          break;

        case 'H':
          if (!ascii_is_digit (at[0]) 
           || !ascii_is_digit (at[1]))
            {
              dsk_set_error (error, "expected two digit hour (at offset %u)",
                             (unsigned)(at-str));
              return DSK_FALSE;
            }
          out->hour = (at[0] - '0') * 10 + (at[1] - '0');
          at += 2;
          format += 2;
          break;
        case 'M':
          if (!ascii_is_digit (at[0]) 
           || !ascii_is_digit (at[1]))
            {
              dsk_set_error (error, "expected two digit minute (at offset %u)",
                             (unsigned)(at-str));
              return DSK_FALSE;
            }
          out->minute = (at[0] - '0') * 10 + (at[1] - '0');
          at += 2;
          format += 2;
          break;
        case 'S':
          if (!ascii_is_digit (at[0]) 
           || !ascii_is_digit (at[1]))
            {
              dsk_set_error (error, "expected two digit second (at offset %u)",
                             (unsigned)(at-str));
              return DSK_FALSE;
            }
          out->second = (at[0] - '0') * 10 + (at[1] - '0');
          at += 2;
          format += 2;
          break;
        case 'Z':
          /* timezone */
          ...
          break;
        }   
    }
  if (end)
    *end = (char*) at;
  return DSK_TRUE;
}



/* Parse either RFC 822/1123 dates (Sun, 06 Nov 1994 08:49:37 GMT)
 * or RFC 850/1036 dates (Sunday, 06-Nov-94 08:49:37 GMT)
 * or ANSI C dates (Sun Nov  6 08:49:37 1994)
 * or ISO 8601 dates (2009-02-12 T 14:32:61.1+01:00) (see RFC 3339)
 */
dsk_boolean dsk_date_parse   (const char *str,
                              char      **end,
                              DskDate    *out,
                              DskError  **error)
{
  unsigned n_alpha = 0;
  const char *at;
  while (ascii_is_alpha (str[n_alpha]))
    n_alpha++;
  at = str + n_alpha;
  if (n_alpha == 3 && str[3] == ',')
    {
      /* RFC 822/1123 */
      return parse_generic ("%A , %d %b %Y %H:%M:%S %Z", str, end, out, error);
    }
  else if (n_alpha == 3 && str[3] == ' ')
    {
      /* ANSI C */
      out->zone_offset = 0;
      return parse_generic ("%A %b %D %H:%M:%S %Y", str, end, out, error);
    }
  else if (n_alpha > 3 && str[n_alpha] == ',')
    {
      /* RFC 850/1036 */
      ...
    }
  else if (n_alpha == 0
           && ascii_is_digit (str[0])
           && ascii_is_digit (str[1])
           && ascii_is_digit (str[2])
           && ascii_is_digit (str[3])
           && str[4] == '-')
    {
      /* ISO 8601 */
      ...
    }
  else
    {
      ...
    }
}

/* 'unixtime' here is seconds since epoch.
   If the date is before the epoch (Jan 1, 1970 00:00 GMT),
   then it is negative. */
/* Behavior of dsk_date_to_unixtime() is undefined if any
   of the fields are out-of-bounds.
   Behavior of dsk_unixtime_to_date() is defined for any time
   whose year fits in an unsigned integer. (therefore, no B.C. dates)
 */
/* dsk_unixtime_to_date() always sets the date_out->zone_offset to 0 (ie GMT) */
int64_t     dsk_date_to_unixtime (DskDate *date);
void        dsk_unixtime_to_date (int64_t  unixtime,
                                  DskDate *date_out);

/* we recognise: UT, UTC, GMT; EST EDT CST CDT MST MDT PST PDT [A-Z]
   and the numeric formats: +#### and -#### */
dsk_boolean dsk_date_parse_timezone (const char *at,
                                     char **end,
				     int *zone_offset_out);


/* UNIMPLEMENTED:  this is the API to deal with localtime
   fully..  To use it requires being able to parse zoneinfo files.
   Which is too much work for a feature we basically don't need. */
DskTimezone *dsk_timezone_get  (const char *name,
                                DskError  **error);
void        dsk_timezone_query (DskTimezone *timezone,
                                gint64       unixtime,
                                int         *minutes_offset_out,
                                const char **timezone_name_out,
                                DskError   **error);
