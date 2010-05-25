#include "dsk.h"

DskHttpRequest *
dsk_http_request_new (DskHttpRequestOptions *options)
{
  unsigned aligned_alloc = 0;
  unsigned str_alloc = 0;

  /* Pass 1:  compute memory needed */
  if (options->full_path != NULL)
    str_alloc += strlen (options->full_path) + 1;
  else if (options->path != NULL)
    {
      str_alloc += strlen (options->path) + 1;
      if (options->query)
        str_alloc += strlen (options->query) + 1;
    }
  else
    dsk_return_val_if_reached (NULL, NULL);

  if (options->content_type)
    {
      ...
    }
  else if (options->content_main_type)
    {
      ...
    }
  if (options->referrer)
    str_alloc += strlen (options->referrer) + 1;
  if (options->user_agent)
    str_alloc += strlen (options->user_agent) + 1;
  for (i = 0; i < options->n_misc_headers * 2; i++)
    str_alloc += strlen (options->misc_headers[i]) + 1;
  aligned_alloc += options->n_misc_headers * 2 * sizeof (char*);

  /* allocate memory */
  slab = dsk_malloc (aligned_alloc + str_alloc);
  aligned_at = slab;
  str_at = slab + aligned_alloc;

  /* phase 2:  initialize structure */
  request = dsk_object_new (&dsk_http_request_class);
  request->_slab = slab;
  ...

  return request;
}

DskHttpResponse *
dsk_http_response_new (DskHttpResponseOptions *options)
{
  ...
}
