#include "dsk.h"

#define UNESCAPED_CHAR  32

typedef struct _DskOctetFilterCQuoterClass DskOctetFilterCQuoterClass;
typedef struct _DskOctetFilterCQuoter DskOctetFilterCQuoter;
struct _DskOctetFilterCQuoterClass
{
  DskOctetFilterClass base_class;
};
struct _DskOctetFilterCQuoter
{
  DskOctetFilter base_instance;
  uint8_t last_char;
};
static void dsk_octet_filter_c_quoter_init (DskOctetFilterCQuoter *cquoter)
{
  cquoter->last_char = UNESCAPED_CHAR;
}
#define dsk_octet_filter_c_quoter_finalize NULL
static void write_octal (DskBuffer *buffer, uint8_t c)
{
  char buf[4];
  buf[0] = '\\';
  if (c < 8)
    {
      buf[1] = '0' + c;
      dsk_buffer_append (buffer, 2, buf);
    }
  else if (c < 64)
    {
      buf[1] = '0' + c/8;
      buf[2] = '0' + c%8;
      dsk_buffer_append (buffer, 3, buf);
    }
  else
    {
      buf[1] = '0' + c/64;
      buf[2] = '0' + c/8%8;
      buf[3] = '0' + c%8;
      dsk_buffer_append (buffer, 4, buf);
    }
}
static dsk_boolean
dsk_octet_filter_c_quoter_process (DskOctetFilter *filter,
                                   DskBuffer      *out,
                                   unsigned        in_length,
                                   const uint8_t  *in_data,
                                   DskError      **error)
{
  DskOctetFilterCQuoter *cquoter = (DskOctetFilterCQuoter *) filter;
  uint8_t last_char = cquoter->last_char;
  DSK_UNUSED (error);
  if (in_length == 0)
    return DSK_TRUE;
  if (last_char == UNESCAPED_CHAR)
    {
unescaped_char_loop:
      {
        const uint8_t *in_start = in_data;
        while (in_length > 0 && ' ' <= *in_data && *in_data <= 126
               && *in_data != '"' && *in_data != '\\')
          in_data++, in_length--;
        if (in_start < in_data)
          dsk_buffer_append (out, in_data - in_start, in_start);
        if (in_length == 0)
          {
            cquoter->last_char = UNESCAPED_CHAR;
            return DSK_TRUE;
          }
        else
          {
            /* handle single-character escapes */
            switch (*in_data)
              {
#define WRITE_CASE_SIMPLE_ESCAPED(c, str) \
            case c: \
              dsk_buffer_append (out, 2, str);  \
              in_data++; \
              if (--in_length == 0) \
                { \
                  cquoter->last_char = UNESCAPED_CHAR; \
                  return DSK_TRUE; \
                } \
              goto unescaped_char_loop
#define WRITE_SINGLE_CHAR_ESCAPED_CASES \
            WRITE_CASE_SIMPLE_ESCAPED ('\a', "\\a"); \
            WRITE_CASE_SIMPLE_ESCAPED ('\b', "\\b"); \
            WRITE_CASE_SIMPLE_ESCAPED ('\f', "\\f"); \
            WRITE_CASE_SIMPLE_ESCAPED ('\n', "\\n"); \
            WRITE_CASE_SIMPLE_ESCAPED ('\r', "\\r"); \
            WRITE_CASE_SIMPLE_ESCAPED ('\t', "\\t"); \
            WRITE_CASE_SIMPLE_ESCAPED ('\v', "\\v"); \
            WRITE_CASE_SIMPLE_ESCAPED ('"' , "\\\""); \
            WRITE_CASE_SIMPLE_ESCAPED ('\\', "\\\\");
                WRITE_SINGLE_CHAR_ESCAPED_CASES
              }

            last_char = *in_data++;
            in_length--;
            goto escaped_char_loop;
          }
      }
    }
  else
    {
escaped_char_loop:
      while (in_length > 0)
        {
          switch (*in_data)
            {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
              {
                /* Write three-octal variant */
                char buf[5] = { '\\',
                                '0' + (*in_data>>6),
                                '0' + ((*in_data>>3)&7),
                                '0' + ((*in_data)&7),
                                *in_data };
                dsk_buffer_append (out, 5, buf);
                in_data++;
                in_length--;
                goto unescaped_char_loop;
              }
            WRITE_SINGLE_CHAR_ESCAPED_CASES
#undef WRITE_CASE_SIMPLE_ESCAPED
#undef WRITE_SINGLE_CHAR_ESCAPED_CASES
            default:
              if (' ' < *in_data || *in_data > 126)
                {
                  write_octal (out, last_char);
                  last_char = *in_data++;
                  in_length--;
                }
              else
                {
                  write_octal (out, last_char);
                  last_char = UNESCAPED_CHAR;
                  goto unescaped_char_loop;
                }
            }
        }
      cquoter->last_char = last_char;
      return DSK_TRUE;
    }
}

static dsk_boolean
dsk_octet_filter_c_quoter_finish  (DskOctetFilter *filter,
                                   DskBuffer      *out,
                                   DskError      **error)
{
  DskOctetFilterCQuoter *cquoter = (DskOctetFilterCQuoter *) filter;
  DSK_UNUSED (error);
  if (cquoter->last_char != UNESCAPED_CHAR)
    write_octal (out, cquoter->last_char);
  return DSK_TRUE;
}

DSK_OCTET_FILTER_SUBCLASS_DEFINE(static, DskOctetFilterCQuoter, dsk_octet_filter_c_quoter);

DskOctetFilter *
dsk_c_quoter_new (void)
{
  return dsk_object_new (&dsk_octet_filter_c_quoter_class);
}
