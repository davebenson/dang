#include <string.h>
#include "dsk.h"


typedef struct _ParseInfo ParseInfo;
struct _ParseInfo
{
  char *slab;
  unsigned n_kv_pairs;
  char **kv_pairs;

  void *free_list;
  unsigned scratch_remaining;
  char *scratch;
};

static void
parse_info_clear (ParseInfo *pi)
{
  void *at = pi->free_list;
  while (at != NULL)
    {
      void *next = * (void **) at;
      dsk_free (at);
      at = next;
    }
}
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
      dsk_set_error (error, "HTTP header too short");
      return DSK_FALSE;
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
      &&   pi->slab[header_len-3] == '\n')
    header_len -= 2;

  /* add NUL */
  pi->slab[header_len] = 0;

  /* count newlines */
  unsigned n_newlines;
  unsigned i;
  char *at;
  n_newlines = 0;
  at = pi->slab;
  for (i = 0; i < header_len; i++)
    if (at[i] == '\n')
      n_newlines++;

  /* align scratch pad */
  pi->kv_pairs = parse_info_alloc (pi, sizeof (char *) * (n_newlines - 1) * 2);

  /* gather key/value pairs, lowercasing keys */
  at = pi->slab;
  at = strchr (at, '\n');    /* skip initial line (not key-value format) */
  dsk_assert (at != NULL);
  at++;

  /* is that the end of the header? */
  if (*at == 0)
    {
      pi->n_kv_pairs = 0;
      return DSK_TRUE;
    }

  /* first key-value line must not be whitespace */
  if (*at == ' ' || *at == '\t')
    {
      dsk_set_error (error, "unexpected whitespace after first HTTP header line");
      goto FAIL;
    }

  unsigned n_kv;
  n_kv = 0;
  while (*at != 0)
    {
      static uint8_t ident_chartable[64] = {
#include "dsk-http-ident-chartable.inc"
      };
      /* note key location */
      pi->kv_pairs[n_kv*2] = at;

      /* lowercase until ':' (test character validity) */
      for (;;)
        {
          uint8_t a = *at;
          switch ((ident_chartable[a/4] >> (a%4*2)) & 3)
            {
            case 0: /* invalid */
              dsk_set_error (error, "invalid character '%c' 0x%02x in HTTP header", a,a);
              goto FAIL;
            case 1: /* passthough */
              at++;
              continue;
            case 2: /* lowercase */
              *at += 'a' - 'A';
              at++;
              continue;
            case 3: /* colon */
              goto at_colon;
            }
        }
    at_colon:
      at++;
      while (*at == ' ' || *at == '\t')
        at++;
      pi->kv_pairs[n_kv*2+1] = at;
      n_kv++;

      char *end = NULL;
    scan_value_end_line:
      /* skip to cr-nl */
      while (*at != '\r' && *at != '\n')
        at++;
      end = at;
      if (*at == '\r')
        {
          at++;
          if (*at != '\n')
            {
              dsk_set_error (error, "got CR without LF in HTTP header");
              return DSK_FALSE;
            }
        }
      at++;             /* skip \n */

      /* if a line begins with whitespace,
         then it is actually a continuation of this header.
         Never seen in the wild, this appears in the HTTP spec a lot. */
      if (*at == ' ' || *at == '\t')
        goto scan_value_end_line;

      /* mark end of value */
      *end = 0;
    }
  pi->n_kv_pairs = n_kv;
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
  char *at;
  DskHttpRequestOptions options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
  if (!parse_info_init (&pi, buffer, header_len, 
                        sizeof (scratch), scratch,
                        error))
    return NULL;

  /* parse first-line */
  switch (pi.slab[0])
    {
    case 'h': case 'H':
      if ((pi.slab[1] == 'e' || pi.slab[1] == 'E')
       || (pi.slab[2] == 'a' || pi.slab[2] == 'A')
       || (pi.slab[3] == 'd' || pi.slab[3] == 'D'))
        {
          options.verb = DSK_HTTP_VERB_HEAD;
          at = pi.slab + 4;
          break;
        }
      goto handle_unknown_verb;
    case 'g': case 'G':
      if ((pi.slab[1] == 'e' || pi.slab[1] == 'E')
       || (pi.slab[2] == 't' || pi.slab[2] == 'T'))
        {
          options.verb = DSK_HTTP_VERB_GET;
          at = pi.slab + 3;
          break;
        }
      goto handle_unknown_verb;
    case 'p': case 'P':
      if ((pi.slab[1] == 'o' || pi.slab[1] == 'O')
       || (pi.slab[2] == 's' || pi.slab[2] == 'S')
       || (pi.slab[3] == 't' || pi.slab[3] == 'T'))
        {
          options.verb = DSK_HTTP_VERB_POST;
          at = pi.slab + 4;
          break;
        }
      if ((pi.slab[1] == 'u' || pi.slab[1] == 'U')
       || (pi.slab[2] == 't' || pi.slab[2] == 'T'))
        {
          options.verb = DSK_HTTP_VERB_PUT;
          at = pi.slab + 3;
          break;
        }
      goto handle_unknown_verb;
    case 'c': case 'C':
      if ((pi.slab[1] == 'o' || pi.slab[1] == 'O')
       || (pi.slab[2] == 'n' || pi.slab[2] == 'N')
       || (pi.slab[3] == 'n' || pi.slab[3] == 'N')
       || (pi.slab[4] == 'e' || pi.slab[4] == 'E')
       || (pi.slab[5] == 'c' || pi.slab[5] == 'C')
       || (pi.slab[6] == 't' || pi.slab[6] == 'T'))
        {
          options.verb = DSK_HTTP_VERB_CONNECT;
          at = pi.slab + 7;
          break;
        }
      goto handle_unknown_verb;
    case 'd': case 'D':
      if ((pi.slab[1] == 'e' || pi.slab[1] == 'E')
       || (pi.slab[2] == 'l' || pi.slab[2] == 'L')
       || (pi.slab[3] == 'e' || pi.slab[3] == 'E')
       || (pi.slab[4] == 't' || pi.slab[4] == 'T')
       || (pi.slab[5] == 'e' || pi.slab[5] == 'E'))
        {
          options.verb = DSK_HTTP_VERB_DELETE;
          at = pi.slab + 6;
          break;
        }
      goto handle_unknown_verb;
    case 'o': case 'O':
      if ((pi.slab[1] == 'p' || pi.slab[1] == 'P')
       || (pi.slab[2] == 't' || pi.slab[2] == 'T')
       || (pi.slab[3] == 'i' || pi.slab[3] == 'I')
       || (pi.slab[4] == 'o' || pi.slab[4] == 'O')
       || (pi.slab[5] == 'n' || pi.slab[5] == 'N')
       || (pi.slab[6] == 's' || pi.slab[6] == 'S'))
        {
          options.verb = DSK_HTTP_VERB_OPTIONS;
          at = pi.slab + 7;
          break;
        }
      goto handle_unknown_verb;
    default: handle_unknown_verb:
      {
        at = pi.slab;
        while (dsk_ascii_isalnum (*at))
          at++;
        *at = 0;
        dsk_set_error (error, "unknown verb '%s' in HTTP request", pi.slab);
        parse_info_clear (&pi);
        return DSK_FALSE;
      }
    }

  /* skip space after verb */
  while (*at == ' ' || *at == '\t')
    at++;

  /* scan path */
  for (;;)
    {
      while (*at && *at != ' ' && *at != '\t')
        at++;
      if (*at == 0)
        {
          dsk_set_error (error, "missing HTTP after PATH");
          parse_info_clear (&pi);
          return NULL;
        }
      while (*at == ' ' || *at == '\t')
        at++;
      if ((at[0] == 'h' || at[0] == 'H')
       && (at[1] == 't' || at[1] == 'T')
       && (at[2] == 't' || at[2] == 'T')
       && (at[3] == 'p' || at[3] == 'P')
       &&  at[4] == '/')
        {
          break;
        }
    }

  /* parse HTTP version */
  at += 5;              /* skip 'http/' */
  if (!dsk_ascii_isdigit (at[0])
    || at[1] != '.'
    || !dsk_ascii_isdigit (at[2]))
    {
      dsk_set_error (error, "error parsing HTTP version");
      parse_info_clear (&pi);
      return NULL;
    }
  options.http_major_version = at[0] - '0';
  options.http_minor_version = at[2] - '0';

  /* parse key-value pairs */
  for (i = 0; i < pi.n_kv_pairs; i++)
    {
      /* name has already been lowercased */
      const char *name = pi.kv_pairs[2*i];
      unsigned name_len = strlen (name);
#define CASE(first, second, count, last) \
      case ((int)first<<24) + ((int)second<<16) + (count)<<8 + (int)(last)
      CASE('c', 'o', 14, 'h'):
        if (strcmp (name, "content-length") == 0)
          {
            if (!handle_content_length (value, &options.content_length, error))
              goto FAIL;
            continue;
          }
        break;

    }

  rv = dsk_http_request_new (&options);
  parse_info_clear (&pi);
  return rv;
}
