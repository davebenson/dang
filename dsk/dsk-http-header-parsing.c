#include "dsk.h"


typedef struct _ParseInfo ParseInfo;
struct _ParseInfo
{
  char *slab;
  char **kv_pairs;

  void *free_list;
  unsigned scratch_remaining;
  char *scratch;
};

static void *
parse_info_alloc (ParseInfo *pi, unsigned size)
{
  /* round 'size' up to a multiple of the size of a pointer */
  if (size & (sizeof(void*)-1))
    size += sizeof(void*) - (size & (sizeof(void*)-1));
  if (size > pi->scratch_remaining)
    {
      void *rv = dsk_malloc (sizeof (void*) + size);
      *(void**)rv = pi->free_list;
      pi->free_list = rv;
      return ((void**)rv) + 1;
    }
  else
    {
      void *rv = pi->scratch;
      pi->scratch += size;
      return rv;
    }
}

static dsk_boolean
parse_info_init (ParseInfo *pi,
                 DskBuffer *buffer,
                 unsigned   header_len,
                 unsigned   scratch_len,
                 char      *scratch_pad,
                 DskError **error)
{
  if (header_len < 4)
    {
      ...
    }
  pi->scratch_remaining = scratch_len;
  pi->scratch = scratch_pad;
  pi->free_list = NULL;
  pi->slab = parse_info_alloc (pi, header_len+1);
  dsk_buffer_peek (buffer, header_len, pi->slab);

  /* get rid of blank terminal line, if supplied */
  if (pi->slab[header_len-1] == '\n'
   && pi->slab[header_len-2] == '\n')
    header_len--;
  else if (pi->slab[header_len-1] == '\n'
      &&   pi->slab[header_len-2] == '\r'
      &&   pi->slab[header_len-3] == '\n'
    header_len -= 2;

  /* add NUL */
  pi->slab[header_len] = 0;

  /* count newlines */
  n_newlines = 0;
  for (i = 0; i < header_len; i++)
    if (hdr[i] == '\n')
      n_newlines++;

  /* align scratch pad */
  pi->kv_pairs = parse_info_alloc (pi, sizeof (char *) * (n_newlines - 1) * 2);

  /* gather key/value pairs, lowercasing keys */
  at = slab;
  at = strchr (at, '\n');    /* skip initial line (not key-value format) */
  dsk_assert (at != NULL);
  at++;

  /* is that the end of the header? */
  ...

  /* first key-value line must not be whitespace */
  ...

  n_kv = 0;
  while (*at != 0)
    {
      /* note key location */
      pi->kv_pairs[n_kv*2] = at;

      /* lowercase until ':' (test character validity) */
      ...

      /* skip whitespace */
      ...

      pi->kv_pairs[kvline*2+1] = at;
      kvline++;

      /* skip to cr-nl */
      while (*at != '\r' && *at != '\n')
        at++;
      end = at;
      if (*at == '\r')
        {
          at++;
          if (*at != '\n')
            {
              ...
            }
        }
      at++;             /* skip \n */
      if (*at == 0)
        {
          *end = 0;
        }
      ...


    }
  ...
  return DSK_TRUE;

FAIL:
  parse_info_clear (pi);
  return DSK_FALSE;
}

DskHttpRequest  *
dsk_http_request_parse_buffer  (DskBuffer *buffer,
                                unsigned   header_len,
                                DskError **error)
{
  char scratch[4096];
  ParseInfo pi;
  DskHttpRequestOptions options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
  if (!parse_info_init (&pi, buffer, header_len, 
                        sizeof (scratch), scratch,
                        error))
    return NULL;

  /* parse first-line */
  switch (pi->slab[0])
    {
    case 'h': case 'H':
      if ((pi->slab[1] == 'e' || pi->slab[1] == 'E')
       || (pi->slab[2] == 'a' || pi->slab[2] == 'A')
       || (pi->slab[3] == 'd' || pi->slab[3] == 'D'))
        {
          ...
        }
      goto handle_unknown_verb;
    case 'g': case 'G':
      if ((pi->slab[1] == 'e' || pi->slab[1] == 'E')
       || (pi->slab[2] == 't' || pi->slab[2] == 'T'))
        {
          ...
        }
      goto handle_unknown_verb;
    case 'p': case 'P':
      if ((pi->slab[1] == 'o' || pi->slab[1] == 'O')
       || (pi->slab[2] == 's' || pi->slab[2] == 'S')
       || (pi->slab[3] == 't' || pi->slab[3] == 'T'))
        {
          ...
        }
      if ((pi->slab[1] == 'u' || pi->slab[1] == 'U')
       || (pi->slab[2] == 't' || pi->slab[2] == 'T'))
        {
          ...
        }
      goto handle_unknown_verb;
    case 'c': case 'C':
      if ((pi->slab[1] == 'o' || pi->slab[1] == 'O')
       || (pi->slab[2] == 'n' || pi->slab[2] == 'N')
       || (pi->slab[3] == 'n' || pi->slab[3] == 'N')
       || (pi->slab[4] == 'e' || pi->slab[4] == 'E')
       || (pi->slab[5] == 'c' || pi->slab[5] == 'C')
       || (pi->slab[6] == 't' || pi->slab[6] == 'T'))
        {
          ...
        }
      goto handle_unknown_verb;
    case 'd': case 'D':
      if ((pi->slab[1] == 'e' || pi->slab[1] == 'E')
       || (pi->slab[2] == 'l' || pi->slab[2] == 'L')
       || (pi->slab[3] == 'e' || pi->slab[3] == 'E')
       || (pi->slab[4] == 't' || pi->slab[4] == 'T')
       || (pi->slab[5] == 'e' || pi->slab[5] == 'E'))
        {
          ...
        }
      goto handle_unknown_verb;
    case 'o': case 'O':
      if ((pi->slab[1] == 'p' || pi->slab[1] == 'P')
       || (pi->slab[2] == 't' || pi->slab[2] == 'T')
       || (pi->slab[3] == 'i' || pi->slab[3] == 'I')
       || (pi->slab[4] == 'o' || pi->slab[4] == 'O')
       || (pi->slab[5] == 'n' || pi->slab[5] == 'N')
       || (pi->slab[6] == 's' || pi->slab[6] == 'S'))
        {
          ...
        }
      goto handle_unknown_verb;
    default: handle_unknown_verb:
      {
        ...
      }
    }

  /* parse key-value pairs */
  ...

  rv = dsk_http_request_new (&options);
  parse_info_clear (&pi);
  return rv;
}
