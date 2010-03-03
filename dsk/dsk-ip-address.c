#include <string.h>
#include <stdlib.h>
#include "dsk-common.h"
#include "dsk-ip-address.h"


#define SKIP_WHITESPACE(str)   while (*str == ' ' || *str == '\t') str++
#define C_IN_RANGE(c,min,max)   ((min) <= (c) && (c) <= (max))
#define ASCII_IS_DIGIT(c)  C_IN_RANGE(c,'0','9')
#define ASCII_IS_XDIGIT(c)  ( C_IN_RANGE(c,'0','9') || C_IN_RANGE(c,'a','f') || C_IN_RANGE(c,'A','F') )

dsk_boolean
dsk_hostname_looks_numeric (const char *str)
{
  unsigned i;
  const char *at;
  at = str;
  SKIP_WHITESPACE (at);
  for (i = 0; i < 3; i++)
    {
      if (!ASCII_IS_DIGIT (*at))
        goto is_not_ipv4;
      while (ASCII_IS_DIGIT (*at))
        at++;
      SKIP_WHITESPACE (at);
      if (*at != '.')
        goto is_not_ipv4;
      at++;
      SKIP_WHITESPACE (at);
    }
  if (!ASCII_IS_DIGIT (*at))
    goto is_not_ipv4;
  while (ASCII_IS_DIGIT (*at))
    at++;
  SKIP_WHITESPACE (at);
  if (*at != 0)
    goto is_not_ipv4;
  return DSK_TRUE;              /* ipv4 */

is_not_ipv4:
  for (i = 0; i < 4; i++)
    {
      unsigned j;
      for (j = 0; j < 4; j++)
        {
          if (!ASCII_IS_XDIGIT (*at))
            goto is_not_ipv6;
          at++;
        }
      if (*at != ':')
        goto is_not_ipv6;
      at++;
    }
  return DSK_TRUE;

is_not_ipv6:
  return DSK_FALSE;
}

dsk_boolean
dsk_ip_address_parse_numeric (const char *str,
                               DskIpAddress *out)
{
  if (strchr (str, '.') == NULL)
    {
      unsigned n = 0;
      out->type = DSK_IP_ADDRESS_IPV6;
      while (*str)
        {
          long v;
          char *end;
          if (*str == ':')
            str++;
          if (*str == 0)
            break;
          v = strtoul (str, &end, 16);
          if (n == 16)
            return DSK_FALSE;
          out->address[n++] = v>>8;
          out->address[n++] = v;
          str = end;
        }
      while (n < 16)
        out->address[n++] = 0;
    }
  else
    {
      /* dotted quad notation */
      char *end;
      out->type = DSK_IP_ADDRESS_IPV4;
      out->address[0] = strtoul (str, &end, 10);
      if (*end != '.')
        return DSK_FALSE;
      str = end + 1;
      out->address[1] = strtoul (str, &end, 10);
      if (*end != '.')
        return DSK_FALSE;
      str = end + 1;
      out->address[2] = strtoul (str, &end, 10);
      if (*end != '.')
        return DSK_FALSE;
      str = end + 1;
      out->address[3] = strtoul (str, &end, 10);
    }
  return DSK_TRUE;
}

