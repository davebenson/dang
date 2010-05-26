#include <string.h>
#include "../dsk.h"

typedef struct _RequestData RequestData;
struct _RequestData
{
  DskMemorySource *source;
  DskMemorySink *sink;
  DskHttpResponse *response_header;
  dsk_boolean content_complete;
  dsk_boolean destroyed;
};
#define REQUEST_DATA_DEFAULT { NULL, NULL, NULL,        \
                               DSK_FALSE,       /* content_complete */ \
                               DSK_FALSE,       /* destroyed */ \
                             }

static void
request_data__handle_response (DskHttpClientStreamTransfer *xfer)
{
  RequestData *rd = xfer->user_data;
  rd->response_header = dsk_object_ref (xfer->response);
}
static void
request_data__handle_content_complete (DskHttpClientStreamTransfer *xfer)
{
  RequestData *rd = xfer->user_data;
  rd->content_complete = DSK_TRUE;
}
static void
request_data__destroy (DskHttpClientStreamTransfer *xfer)
{
  RequestData *rd = xfer->user_data;
  rd->destroyed = DSK_TRUE;
}

static void
request_data_clear (RequestData *rd)
{
  dsk_object_unref (rd->source);
  dsk_object_unref (rd->sink);
  if (rd->response_header)
    dsk_object_unref (rd->response_header);
}

static dsk_boolean
is_http_request_complete (DskBuffer *buf)
{
  char *slab = dsk_malloc (buf->size + 1);
  dsk_boolean rv;
  dsk_warning ("is_http_request_complete");

  dsk_buffer_peek (buf, buf->size, slab);
  slab[buf->size] = 0;

  rv = strstr (slab, "\n\n") != NULL
    || strstr (slab, "\n\r\n") != NULL;
  dsk_free (slab);
  return rv;
}

static void
test_simple (void)
{
  DskHttpClientStream *stream;
  DskHttpClientStreamOptions options = DSK_HTTP_CLIENT_STREAM_OPTIONS_DEFAULT;
  RequestData request_data = REQUEST_DATA_DEFAULT;
  DskHttpRequestOptions req_options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
  DskHttpRequest *request;
  DskHttpClientStreamTransfer *xfer;
  DskHttpClientStreamFuncs request_funcs_0;
  DskError *error = NULL;
  memset (&request_funcs_0, 0, sizeof (request_funcs_0));
  request_funcs_0.handle_response = request_data__handle_response;
  request_funcs_0.handle_content_complete = request_data__handle_content_complete;
  request_funcs_0.destroy = request_data__destroy;
  request_data.source = dsk_memory_source_new ();
  request_data.sink = dsk_memory_sink_new ();
  request_data.sink->max_buffer_size = 100000000;
  stream = dsk_http_client_stream_new (DSK_OCTET_SINK (request_data.sink),
                                       DSK_OCTET_SOURCE (request_data.source),
                                       &options);
  req_options.host = "localhost";
  req_options.full_path = "/hello.txt";
  request = dsk_http_request_new (&req_options, &error);
  xfer = dsk_http_client_stream_request (stream, request, NULL, 
                                         &request_funcs_0, &request_data);

  /* read data from sink */
  while (!is_http_request_complete (&request_data.sink->buffer))
    dsk_main_run_once ();

  dsk_warning ("got request");

  /* write response */
  dsk_buffer_append_string (&request_data.source->buffer,
                            "HTTP/1.1 200 OK\r\n"
                            "Date: Mon, 17 May 2010 22:50:08 GMT\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: 7\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "hi mom\n");
  dsk_memory_source_added_data (request_data.source);

  while (request_data.response_header == NULL)
    dsk_main_run_once ();
  dsk_assert (request_data.response_header->http_major_version == 1);
  dsk_assert (request_data.response_header->http_minor_version == 1);
  dsk_assert (request_data.response_header->content_length == 7);
  dsk_assert (request_data.response_header->connection_close);
  while (!request_data.content_complete)
    dsk_main_run_once ();

  dsk_assert (request_data.sink->buffer.size == 7);
  {
    char buf[7];
    dsk_buffer_peek (&request_data.sink->buffer, 7, buf);
    dsk_assert (memcmp (buf, "hi mom", 7) == 0);
  }

  request_data_clear (&request_data);
}

int main(void)
{
  test_simple ();
  return 0;
}
