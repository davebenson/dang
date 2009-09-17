#include "dang.h"

unsigned dang_utf8_encode (uint32_t unicode,
                           char    *buf)
{
  if (unicode < 128)
    {
      *buf = unicode;
      return 1;
    }
  else if (unicode < 0x800)
    {
      buf[0] = (unicode>>6) | 0xc0;
      buf[1] = (unicode & 0x3f) | 0x80;
      return 2;
    }
  else if (unicode < 0x10000)
    {
      buf[0] = (unicode>>12) | 0xe0;
      buf[1] = ((unicode>>6) | 0x80) & 0xbf;
      buf[2] = (unicode & 0x3f) | 0x80;
      return 3;
    }
  else //if (unicode < 0x10ffff)
    {
      buf[0] = ((unicode>>18) & 0x07) | 0xf0;
      buf[1] = ((unicode>>12) | 0x80) & 0xbf;
      buf[2] = ((unicode>>6) | 0x80) & 0xbf;
      buf[3] = ((unicode>>0) | 0x80) & 0xbf;
      return 4;
    }
}

dang_boolean dang_utf8_scan_char (char **at_inout,
                                  unsigned      max_len,
                                  dang_unichar *char_out,
                                  DangError   **error)
{
  const unsigned char *a = * (const unsigned char **) at_inout;
  if ((*a & 0x80) == 0)
    {
      *at_inout = (char*)a + 1;
      *char_out = *a;
      return TRUE;
    }
  else if ((*a & 0xe0) == 0xc0)
    {
      if (max_len < 2)
        goto too_short;
      *char_out = ((a[0] & 0x1f) << 6) | (a[1] & 0x3f);
      if (*char_out < 128)
        goto invalid_utf8;
      *at_inout = (char*)a + 2;
      return TRUE;
    }
  else if ((*a & 0xf0) == 0xe0)
    {
      if (max_len < 3)
        goto too_short;
      *char_out = ((a[0] & 0x0f) << 12) | ((a[1] & 0x3f) << 6) | 
                  (a[2] & 0x3f);
      if (*char_out < 0x800)
        goto invalid_utf8;
      *at_inout = (char*)a + 3;
      return TRUE;
    }
  else if ((*a & 0xf8) == 0xf0)
    {
      if (max_len < 4)
        goto too_short;
      *char_out = ((a[0] & 0x07) << 18) | ((a[1] & 0x3f) << 12) | 
                  ((a[2] & 0x3f) <<  6) | (a[3] & 0x3f);
      if (*char_out < 0x10000)
        goto invalid_utf8;
      *at_inout = (char*)a + 4;
      return TRUE;
    }
  dang_set_error (error, "unexpected byte '0x%02x' encountered in utf8 data",
                  *a);
  return FALSE;

too_short:
  dang_set_error (error, "premature end-of-string at utf8-encoded char starting at byte %u", (a - (unsigned char*)*at_inout));
  return FALSE;
invalid_utf8:
  dang_set_error (error, "invalid utf8 encountered (possibly encoded with too many bytes)");
  return FALSE;
}

static const char utf8_skip_data[256] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};
const char  *dang_utf8_next_char (const char *str)
{
  unsigned char c = *str;
  if (c == 0 || c == 0xfe || c == 0xff)
    return NULL;
  return str + utf8_skip_data[c];
}
dang_boolean dang_utf8_validate_str (unsigned len,
                                     const char *str,
                                     DangError **error)
{
  const unsigned char *a = (const unsigned char *) str;
  const unsigned char *end = a + len;
  while (a < end)
    {
      if ((*a & 0x80) == 0)
        {
          a++;
        }
      else if ((*a & 0xe0) == 0xc0)
        {
          if (end < a + 2)
            goto premature_end_of_str;
          if ((*a & 0x1e) == 0)
            goto overlong_seq;
          if ((a[1] & 0xc0) != 0x80)
            goto invalid_nonfirst_char;
          a += 2;
        }
      else if ((*a & 0xf0) == 0xe0)
        {
          if (end < a + 3)
            goto premature_end_of_str;
          if (((a[0] & 0xf) | (a[1] & 0x20)) == 0)
            goto overlong_seq;
          if ((a[1] & 0xc0) != 0x80
           || (a[2] & 0xc0) != 0x80)
            goto invalid_nonfirst_char;
          a += 3;
        }
      else if ((*a & 0xf8) == 0xf0)
        {
          if (end < a + 4)
            goto premature_end_of_str;
          if (((a[0] & 0x7) | (a[1] & 0x30)) == 0)
            goto overlong_seq;
          if ((a[1] & 0xc0) != 0x80
           || (a[2] & 0xc0) != 0x80
           || (a[3] & 0xc0) != 0x80)
            goto invalid_nonfirst_char;
          a += 4;
        }
      else
        {
          dang_set_error (error,
                          "disallowed byte 0x%02x at start of utf8-encoded char",
                          *a);
          return FALSE;
        }
    }
  return TRUE;

premature_end_of_str:
  dang_set_error (error, "premature end-of-string at utf8-encoded char starting at byte %u", (a - (unsigned char*)str));
  return FALSE;
overlong_seq:
  dang_set_error (error, "overload encoding encountered starting at byte %u", (a - (unsigned char*)str));
  return FALSE;
invalid_nonfirst_char:
  dang_set_error (error, "multibyte characters nonfirst bytes did not have 10 as their high-order bits, at char starting at byte %u", (a - (unsigned char*)str));
  return FALSE;
}

unsigned dang_utf8_count_unichars        (unsigned            len,
                                          const char         *str)
{
  const unsigned char *s = (const unsigned char *) str;
  unsigned c = 0;
  while (len--)
    {
      if ((*s & 0x80) == 0 || (*s & 0xc0) != 0x80)
        c++;
      s++;
    }
  return c;
}
void     dang_utf8_string_to_unichars    (unsigned            len,
                                          const char         *str,
                                          dang_unichar       *out)
{
  char *s = (char*) str;
  while (len)
    {
      if ((*s & 0x80) == 0)
        {
          *out++ = *s++;
          len--;
        }
      else
        {
          DangError *error = NULL;
          char *old_str = s;
          if (!dang_utf8_scan_char (&s, len, out, &error))
            dang_die ("dang_utf8_scan_char failed: called in invalid utf8: %s", error->message);
          out++;
          len -= s - old_str;
        }
    }
}

unsigned
dang_unichar_array_get_utf8_len (unsigned            n_chars,
                                 const dang_unichar *chars)
{
  unsigned out = 0;
  unsigned i;
  for (i = 0; i < n_chars; i++)
    if (chars[i] < 128)
      out += 1;
    else if (chars[i] < 0x800)
      out += 2;
    else if (chars[i] < 0x10000)
      out += 3;
    else //if (chars[i] < 0x110000)
      out += 4;
  return out;
}

void
dang_unichar_array_to_utf8     (unsigned            n_chars,
                                const dang_unichar *chars,
                                char               *utf8_out)
{
  unsigned i;
  for (i = 0; i < n_chars; i++)
    utf8_out += dang_utf8_encode (chars[i], utf8_out);
}
