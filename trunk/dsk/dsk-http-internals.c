#include "dsk.h"
#include "dsk-http-internals.h"

dsk_boolean
_dsk_http_scan_for_end_of_header (DskBuffer *buffer,
                                  unsigned *checked_inout,
                                  dsk_boolean permit_empty)
{
  unsigned start = *checked_inout;
  DskBufferFragment *frag;
  unsigned frag_offset;
  frag = dsk_buffer_find_fragment (buffer, start, &frag_offset);
  if (frag == NULL)
    return DSK_FALSE;           /* no new data */

  /* state 0:  non-\n
     state 1:  \n
     state 2:  \n \r
   */
  unsigned state = permit_empty ? 1 : 0;
  uint8_t *at = frag->buf + (start - frag_offset) + frag->buf_start;
  while (frag != NULL)
    {
      uint8_t *end = frag->buf + frag->buf_start + frag->buf_length;

      while (at < end)
        {
          if (*at == '\n')
            {
              if (state == 0)
                state = 1;
              else
                {
                  at++;
                  start++;
                  *checked_inout = start;
                  return DSK_TRUE;
                }
            }
          else if (*at == '\r')
            {
              if (state == 1)
                state = 2;
              else
                state = 0;
            }
          else
            state = 0;
          at++;
          start++;
        }

      frag = frag->next;
      if (frag != NULL)
        at = frag->buf + frag->buf_start;
    }

  /* hmm. this could obviously get condensed,
     but that would be some pretty magik stuff. */
  if (start < 2)
    start = 0;
  else if (state == 1)
    start -= 1;
  else if (state == 2)
    start -= 2;
  *checked_inout = start;
  return DSK_FALSE;
}

