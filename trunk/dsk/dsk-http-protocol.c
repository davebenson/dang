#include <string.h>
#include "dsk.h"

typedef struct _ToCopy ToCopy;
struct _ToCopy
{
  unsigned object_offset;   /* offset into DskHttpRequest/Response structure */
  unsigned str_offset;      /* offset in str-heap */
  const char *src;          /* pointer to options structure */
  unsigned length;
};
#define MAX_TO_COPY 10  /* TODO: needs exactly measurement */

typedef struct _StrFixup StrFixup;
struct _StrFixup
{
  unsigned str_offset;
  char c;
};

#define MAX_STR_FIXUPS 10  /* TODO: needs exactly measurement */

static void
apply_copies_and_fixups (void *object,
                         unsigned n_to_copy, ToCopy *to_copy,
                         unsigned n_fixups, StrFixup *fixups,
                         char *str_slab)
{
  unsigned i;
  for (i = 0; i < n_to_copy; i++)
    {
      char *dest = str_slab + to_copy[i].str_offset;
      memcpy (dest, to_copy[i].src, to_copy[i].length);
      if (to_copy[i].object_offset != 0)
        * (char **)((char*)object + to_copy[i].object_offset) = dest;
    }
  for (i = 0; i < n_fixups; i++)
    str_slab[fixups[i].str_offset] = fixups[i].c;
}

static unsigned
phase1_handle_unparsed_headers(unsigned  n_unparsed,
                           char    **unparsed,
                           unsigned *str_alloc_inout,
                           unsigned *aligned_alloc_inout)
{
  unsigned i;
  unsigned unparsed_headers_start = *str_alloc_inout;
  for (i = 0; i < n_unparsed * 2; i++)
    *str_alloc_inout += strlen (unparsed[i]) + 1;
  *aligned_alloc_inout += n_unparsed * 2 * sizeof (char*);
  return unparsed_headers_start;
}

static DskHttpHeaderMisc *
phase2_handle_unparsed_headers (unsigned n_unparsed, char **unparsed,
                            char **aligned_slab_at,
                            char *str_slab_at)
{
  char *at = str_slab_at;
  DskHttpHeaderMisc *unparsed_headers = (DskHttpHeaderMisc *) (*aligned_slab_at);
  unsigned i;
  *aligned_slab_at += sizeof (DskHttpHeaderMisc) * n_unparsed * 2;
  for (i = 0; i < n_unparsed; i++)
    {
      unparsed_headers[i].key = at;
      at = dsk_stpcpy (at, unparsed[i]) + 1;
      unparsed_headers[i].value = at;
      at = dsk_stpcpy (at, unparsed[i]) + 1;
    }
  return unparsed_headers;
}

static dsk_boolean
has_request_body (DskHttpVerb verb)
{
  return (verb == DSK_HTTP_VERB_POST || verb == DSK_HTTP_VERB_PUT);
}


/* Macros shared between request/response new */

/* Add a string to the object given a source string */
#define ADD_STR(src_string, object_member)                                   \
  do{                                                                        \
    const char *the_str = (src_string);                                      \
    unsigned the_str_length = strlen (the_str) + 1;                          \
    to_copy[n_to_copy].length = the_str_length;                              \
    to_copy[n_to_copy].src = the_str;                                        \
    to_copy[n_to_copy].str_offset = str_alloc;                               \
    to_copy[n_to_copy].object_offset = offsetof (ObjectType, object_member); \
    n_to_copy++;                                                             \
    str_alloc += the_str_length;                                             \
  }while(0)
#define APPEND_CHAR_THEN_STR(char_to_append, src_string)                     \
  do{                                                                        \
    const char *the_str = (src_string);                                      \
    unsigned the_str_length = strlen (the_str) + 1;                          \
    fixups[n_fixups].c = char_to_append;                                     \
    fixups[n_fixups].str_offset = str_alloc - 1;                             \
    n_fixups++;                                                              \
    to_copy[n_to_copy].length = the_str_length;                              \
    to_copy[n_to_copy].src = the_str;                                        \
    to_copy[n_to_copy].str_offset = str_alloc;                               \
    to_copy[n_to_copy].object_offset = 0;                                    \
    n_to_copy++;                                                             \
  }while(0)
#define MAYBE_ADD_STR(src_string, object_member)                             \
  do{ if (src_string) ADD_STR (src_string, object_member); } while(0)
#define STR_DEFAULT(opt_str, default_value)                                  \
   ((opt_str) == NULL ? (default_value) : (opt_str))

#define ObjectType DskHttpRequest
DskHttpRequest *
dsk_http_request_new (DskHttpRequestOptions *options,
                      DskError             **error)
{
  unsigned aligned_alloc = 0;
  unsigned str_alloc = 0;
  unsigned n_to_copy = 0;
  ToCopy to_copy[MAX_TO_COPY];
  unsigned n_fixups = 0;
  StrFixup fixups[MAX_STR_FIXUPS];
  DskHttpRequest *request = dsk_object_new (&dsk_http_request_class);


  /* ---- Pass 1:  compute memory needed ---- */
  /* We try to store as much information as we can
     in the "to_copy" and "fixups" arrays,
     to minimize the amount of custom work to be done in phase 2. */
  if (options->full_path != NULL)
    ADD_STR (options->full_path, path);
  else if (options->path != NULL)
    {
      ADD_STR (options->path, path);
      if (options->query != NULL)
        APPEND_CHAR_THEN_STR ('?', options->query);
    }
  else
    dsk_return_val_if_reached (NULL, NULL);

  if (options->content_type)
    {
      ADD_STR (options->content_type, content_type);
    }
  else if (options->content_main_type)
    {
      ADD_STR (options->content_type, content_type);
      APPEND_CHAR_THEN_STR ('/', STR_DEFAULT (options->content_sub_type, "*"));
      if (options->content_charset)
        APPEND_CHAR_THEN_STR ('/', options->content_charset);
    }
  MAYBE_ADD_STR (options->referrer, referrer);
  MAYBE_ADD_STR (options->user_agent, user_agent);

  if (options->has_date)
    {
      request->has_date = 1;
      request->date = options->date;
    }
  if (has_request_body (options->verb))
    {
      if (options->content_length >= 0)
        request->content_length = options->content_length;
      else if (options->http_minor_version >= 1)
        request->transfer_encoding_chunked = 1;
      else
        {
          ...
        }
    }
  ...

  unsigned unparsed_headers_start;
  unparsed_headers_start
    = phase1_handle_unparsed_headers (options->n_unparsed_headers,
                                  options->unparsed_headers,
                                  &str_alloc, &aligned_alloc);

  /* allocate memory */
  char *slab;
  char *aligned_at;
  char *str_slab;
  slab = dsk_malloc (aligned_alloc + str_alloc);
  aligned_at = slab;
  str_slab = slab + aligned_alloc;

  /* ---- phase 2:  initialize structure ---- */
  request->_slab = slab;
  apply_copies_and_fixups (request,
                           n_to_copy, to_copy, n_fixups, fixups,
                           str_slab);

  /* NOTE: any special cases can go here */

  /* - Handle uninterpreted headers - */
  request->n_unparsed_headers = options->n_unparsed_headers;
  request->unparsed_headers
    = phase2_handle_unparsed_headers (options->n_unparsed_headers,
                                  options->unparsed_headers,
                                  &aligned_at,
                                  str_slab + unparsed_headers_start);

  return request;
}
#undef ObjectType 

#define ObjectType DskHttpResponse
DskHttpResponse *
dsk_http_response_new (DskHttpResponseOptions *options,
                       DskError              **error)
{
  ...
}
#undef ObjectType
