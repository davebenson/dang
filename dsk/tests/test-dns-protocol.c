#include "../dsk.h"

int main()
{
  DskDnsMessage *message;

  {
    static const char bindata[] = ""\245\246\1\0\0\1\0\0\0\0\0\0\3www\3foo\3com\0\0\1\0\1"";
    message = dsk_dns_message_parse (sizeof (bindata), (uint8_t*) bindata,
                                     &error);
    dsk_assert (message != NULL);
    dsk_free (message);
  }

  return 0;
}
