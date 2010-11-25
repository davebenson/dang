#include "dsk-common.h"
#include "dsk-utf8.h"

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
