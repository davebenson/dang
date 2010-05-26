#include <string.h>
#include <stdio.h>
#include "../dsk.h"

typedef struct _RequestData RequestData;
struct _RequestData
{
  DskMemorySource *source;
  DskMemorySink *sink;
  DskHttpResponse *response_header;
  dsk_boolean content_complete;
  DskBuffer content;
  dsk_boolean destroyed;
};
#define REQUEST_DATA_DEFAULT { NULL, NULL, NULL,        \
                               DSK_FALSE,       /* content_complete */ \
                               DSK_BUFFER_STATIC_INIT, \
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
  dsk_buffer_drain (&rd->content, &xfer->content->buffer);
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

  dsk_buffer_peek (buf, buf->size, slab);
  slab[buf->size] = 0;

  rv = strstr (slab, "\n\n") != NULL
    || strstr (slab, "\n\r\n") != NULL;
  dsk_free (slab);
  return rv;
}

static void
test_simple (dsk_boolean byte_by_byte)
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

  /* write response */
  static const char *content = 
                            "HTTP/1.1 200 OK\r\n"
                            "Date: Mon, 17 May 2010 22:50:08 GMT\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: 7\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "hi mom\n";
  if (byte_by_byte)
    {
      const char *at = content;
      while (*at)
        {
          dsk_buffer_append_byte (&request_data.source->buffer, *at++);
          dsk_memory_source_added_data (request_data.source);
          while (request_data.source->buffer.size)
            dsk_main_run_once ();
        }
    }
  else
    {
      dsk_buffer_append_string (&request_data.source->buffer, content);
      dsk_memory_source_added_data (request_data.source);
    }

  while (request_data.response_header == NULL)
    dsk_main_run_once ();
  dsk_assert (request_data.response_header->http_major_version == 1);
  dsk_assert (request_data.response_header->http_minor_version == 1);
  dsk_assert (request_data.response_header->content_length == 7);
  dsk_assert (!request_data.response_header->transfer_encoding_chunked);
  dsk_assert (request_data.response_header->connection_close);
  while (!request_data.content_complete)
    dsk_main_run_once ();

  dsk_assert (request_data.content.size == 7);
  {
    char buf[7];
    dsk_buffer_peek (&request_data.content, 7, buf);
    dsk_assert (memcmp (buf, "hi mom\n", 7) == 0);
  }

  request_data_clear (&request_data);
}

static void test_simple_bigwrite() { test_simple(DSK_FALSE); }
static void test_simple_bytebybyte() { test_simple(DSK_TRUE); }

static void
test_transfer_encoding_chunked (void)
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

  /* write response */
  unsigned pass;
  for (pass = 0; pass < 2; pass++)
    {
      static const char *content = 
                            "HTTP/1.1 200 OK\r\n"
                            "Date: Mon, 17 May 2010 22:50:08 GMT\r\n"
                            "Content-Type: text/plain\r\n"
                            "Transfer-Encoding: chunked\r\n"
                            "\r\n"
                            "7\r\nhi mom\n\r\n"
                            "0\r\n"
                            "\r\n"              /* trailer */
                            "\r\n";
      request = dsk_http_request_new (&req_options, &error);
      xfer = dsk_http_client_stream_request (stream, request, NULL, 
                                             &request_funcs_0, &request_data);

      /* read data from sink */
      while (!is_http_request_complete (&request_data.sink->buffer))
        dsk_main_run_once ();

      switch (pass)
        {
        case 0:
          dsk_buffer_append_string (&request_data.source->buffer, content);
          dsk_memory_source_added_data (request_data.source);
          break;
        case 1:
          {
            const char *at = content;
            while (*at)
              {
                dsk_buffer_append_byte (&request_data.source->buffer, *at++);
                dsk_memory_source_added_data (request_data.source);
                while (request_data.source->buffer.size)
                  dsk_main_run_once ();
              }
          }
          break;
        }

      while (request_data.response_header == NULL)
        dsk_main_run_once ();

      while (request_data.response_header == NULL)
        dsk_main_run_once ();
      dsk_assert (request_data.response_header->http_major_version == 1);
      dsk_assert (request_data.response_header->http_minor_version == 1);
      dsk_assert (request_data.response_header->content_length == -1LL);
      dsk_assert (request_data.response_header->transfer_encoding_chunked);
      dsk_assert (!request_data.response_header->connection_close);
      while (!request_data.content_complete)
        dsk_main_run_once ();

      dsk_assert (request_data.content.size == 7);
      {
        char buf[7];
        dsk_buffer_peek (&request_data.content, 7, buf);
        dsk_assert (memcmp (buf, "hi mom\n", 7) == 0);
      }
      dsk_buffer_clear (&request_data.content);

      request_data.content_complete = 0;
      request_data.destroyed = 0;
      dsk_object_unref (request_data.response_header);
      request_data.response_header = NULL;
      dsk_buffer_clear (&request_data.sink->buffer);
    }


  request_data_clear (&request_data);
}


static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple connection-close", test_simple_bigwrite },
  { "simple connection-close byte-by-byte", test_simple_bytebybyte },
  { "transfer-encoding chunked content", test_transfer_encoding_chunked },
  // { "transfer-encoding chunked POST", test_transfer_encoding_chunked_post },
  // { "content-encoding gzip", test_content_encoding_gzip },
};

int main(void)
{
  unsigned i;
  for (i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
      fprintf (stderr, "Test: %s... ", tests[i].name);
      tests[i].test ();
      fprintf (stderr, " done.\n");
    }
  return 0;
}
