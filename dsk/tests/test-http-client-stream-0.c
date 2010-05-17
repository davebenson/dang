#include "../dsk.h"

typedef struct _RequestData RequestData;
struct _RequestData
{
  DskMemorySource *source;
  DskMemorySink *sink;
  DskHttpResponse *response;
  dsk_boolean content_complete;
};
#define REQUEST_DATA_DEFAULT { NULL, NULL, NULL,        \
                               DSK_FALSE        /* content_complete */ \
                             }

static void
test_simple (void)
{
  DskHttpClientStream *stream;
  DskHttpClientStreamOptions options = DSK_HTTP_CLIENT_STREAM_OPTIONS_DEFAULT;
  RequestData request_data = REQUEST_DATA_DEFAULT;
  DskHttpRequestOptions req_options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
  request_data.source = dsk_memory_source_new ();
  request_data.sink = dsk_memory_sink_new ();
  request_data.sink->max_buffer_size = 100000000;
  stream = dsk_http_client_stream_new (sink, source, &options);
  req_options.host = "localhost";
  req_options.full_path = "/hello.txt";
  request = dsk_http_request_new (&req_options);
  xfer = dsk_http_client_stream_request (stream, request, NULL, 
                                         &request_funcs_0, &request_data);

  /* read data from sink */
  while (!is_http_request_complete (request_data.sink->buffer))
    dsk_main_run_once ();

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

  dsk_assert (request_data.content_buffer.size == 7);
  {
    char buf[7];
    dsk_buffer_peek (&request_data.content_buffer, 7, buf);
    dsk_assert (memcmp (buf, "hi mom", 7) == 0);
  }

  request_data_clear (&request_data);
}

int main(int argc, char **argv)
{
  test_simple ();
  return 0;
}
