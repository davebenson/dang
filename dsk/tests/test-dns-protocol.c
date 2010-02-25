#include "../dsk.h"

#  define G_BREAKPOINT()        do{__asm__ __volatile__ ("int $03"); }while(0)


int main()
{
  DskDnsMessage *message;
  DskError *error = NULL;

  {
    static const char bindata[] = "\245\246\1\0\0\1\0\0\0\0\0\0\3www\3foo\3com\0\0\1\0\1";
    message = dsk_dns_message_parse (sizeof (bindata), (uint8_t*) bindata,
                                     &error);
    if (error)
      dsk_warning("error parsing message: %s", error->message);
    dsk_assert (message->is_query);
    dsk_assert (!message->is_truncated);
    dsk_assert (message->recursion_desired);
    dsk_assert (message->n_questions == 1);
    dsk_assert (strcmp (message->questions[0].name, "www.foo.com") == 0);
    dsk_assert (message->questions[0].query_type == DSK_DNS_RR_HOST_ADDRESS);
    dsk_assert (message->questions[0].query_class == DSK_DNS_CLASS_IN);
    dsk_assert (message->n_answer_rr == 0);
    dsk_assert (message->n_authority_rr == 0);
    dsk_assert (message->n_additional_rr == 0);
    dsk_assert (message != NULL);
    dsk_free (message);
  }
  {
    static const char bindata[] = "C\230\201\200\0\1\0\1\0\2\0\0\3www\3foo\3com\0\0\1\0\1\300\f\0\1\0\1\0\0\r\367\0\4@^}\212\300\20\0\2\0\1\0\0\r\367\0\16\2ns\10okdirect\300\24\300\20\0\2\0\1\0\0\r\367\0\6\3ns2\300<";
    message = dsk_dns_message_parse (sizeof (bindata), (uint8_t*) bindata,
                                     &error);
    if (error)
      dsk_warning("error parsing message: %s", error->message);
    dsk_assert (!message->is_query);
    dsk_assert (!message->is_truncated);
    dsk_assert (message->recursion_available);
    G_BREAKPOINT ();

  }

  return 0;
}
