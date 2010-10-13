#include "dsk-common.h"
#include "dsk-unicode.h"

char dsk_ascii_hex_digits[16] = {
  '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
};
unsigned char dsk_ascii_chartable[256] = 
{
#include "dsk-ascii-chartable.inc"
};

/* xdigit_values and digit_values are machine-generated */
#include "dsk-digit-chartables.inc"

int dsk_ascii_xdigit_value (int c)
{
  return xdigit_values[(uint8_t)c];
}

int dsk_ascii_digit_value (int c)
{
  return digit_values[(uint8_t)c];
}
#include "dsk-byte-name-table.inc"
const char *dsk_ascii_byte_name(unsigned char byte)
{
  return byte_name_str + (byte_name_offsets[byte]);
}

void
dsk_utf8_skip_whitespace (const char **p_str)
{
  const unsigned char *str = (const unsigned char *) *p_str;
  while (*str)
    {
      switch (*str)
        {
        case ' ': case '\t': case '\r': case '\n': str++;
        default: *p_str = (const char *) str; return;
                 /* TODO: handle other spaces */
        }
    }
  *p_str = (const char *) str;
}
int dsk_ascii_strcasecmp  (const char *a, const char *b)
{
  while (*a && *b)
    {
      char A = dsk_ascii_tolower (*a);
      char B = dsk_ascii_tolower (*b);
      if (A < B)
        return -1;
      else if (A > B)
        return +1;
      a++;
      b++;
    }
  if (*a)
    return +1;
  else if (*b)
    return -1;
  else
    return 0;
}

int dsk_ascii_strncasecmp (const char *a, const char *b, size_t max_len)
{
  unsigned rem = max_len;
  while (*a && *b && rem)
    {
      char A = dsk_ascii_tolower (*a);
      char B = dsk_ascii_tolower (*b);
      if (A < B)
        return -1;
      else if (A > B)
        return +1;
      a++;
      b++;
      rem--;
    }
  if (rem == 0)
    return 0;
  if (*a)
    return +1;
  else if (*b)
    return -1;
  else
    return 0;
}

void
dsk_ascii_strchomp (char *inout)
{
  char *end;
  for (end = inout; *end; end++)
    {
    }
  while (end > inout && dsk_ascii_isspace (*(end-1)))
    end--;
  *end = '\0';
}
