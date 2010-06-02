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
is_http_request_complete (DskBuffer *buf,
                          unsigned  *length_out)
{
  char *slab = dsk_malloc (buf->size + 1);
  const char *double_nl;

  dsk_buffer_peek (buf, buf->size, slab);
  slab[buf->size] = 0;

  double_nl = strstr (slab, "\n\n");
  if (double_nl != NULL)
    {
      if (length_out)
        *length_out = double_nl - slab + 2;
    }
  else
    {
      double_nl = strstr (slab, "\n\r\n");
      if (length_out)
        *length_out = double_nl - slab + 3;
    }
  dsk_free (slab);
  return double_nl != NULL;
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
  while (!is_http_request_complete (&request_data.sink->buffer, NULL))
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
      while (!is_http_request_complete (&request_data.sink->buffer, NULL))
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

static void
test_simple_post (void)
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
  const char *post_data_str = "this is some POST data\n";
  req_options.verb = DSK_HTTP_VERB_POST;
  req_options.host = "localhost";
  req_options.full_path = "/hello.txt";
  req_options.content_length = strlen (post_data_str);
  request = dsk_http_request_new (&req_options, &error);
  DskMemorySource *post_data;
  post_data = dsk_memory_source_new ();
  dsk_buffer_append_string (&post_data->buffer, post_data_str);
  dsk_memory_source_done_adding (post_data);
  xfer = dsk_http_client_stream_request (stream, request, DSK_OCTET_SOURCE (post_data),
                                         &request_funcs_0, &request_data);
  dsk_object_unref (post_data);

  /* read data from sink; pluck off POST Data */
  unsigned len;
  while (!is_http_request_complete (&request_data.sink->buffer, &len))
    dsk_main_run_once ();
  dsk_buffer_discard (&request_data.sink->buffer, len);
  while (request_data.sink->buffer.size < strlen (post_data_str))
    dsk_main_run_once ();
  dsk_assert (request_data.sink->buffer.size == strlen (post_data_str));
  {
    char slab[1000];
    dsk_buffer_read (&request_data.sink->buffer, 1000, slab);
    dsk_assert (memcmp (slab, post_data_str, strlen (post_data_str)) == 0);
  }

  /* write response */
  static const char *content = 
                            "HTTP/1.1 200 OK\r\n"
                            "Date: Mon, 17 May 2010 22:50:08 GMT\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: 7\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "hi mom\n";
  dsk_buffer_append_string (&request_data.source->buffer, content);
  dsk_memory_source_added_data (request_data.source);

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

typedef struct
{
  DskMemorySource *memory_source;
  const char *str;
} PostInfo;

static void
write_post_data (void *pi)
{
  PostInfo *post_info = pi;

  dsk_buffer_append_string (&post_info->memory_source->buffer,
                            post_info->str);
  dsk_memory_source_added_data (post_info->memory_source);
  dsk_memory_source_done_adding (post_info->memory_source);
  dsk_object_unref (post_info->memory_source);
}
static void
write_post_data_1by1 (void *pi)
{
  PostInfo *post_info = pi;
  dsk_warning ("post_info->str=%s", post_info->str);
  if (*(post_info->str) == 0)
    {
      dsk_memory_source_done_adding (post_info->memory_source);
      dsk_object_unref (post_info->memory_source);
      dsk_warning ("finished writing post data");
      return;
    }

  dsk_warning ("write_post_data_1by1: dsk_buffer_append_byte: %c", *post_info->str);
  dsk_buffer_append_byte (&post_info->memory_source->buffer,
                          *post_info->str);
  post_info->str += 1;
  dsk_memory_source_added_data (post_info->memory_source);
  dsk_main_add_idle (write_post_data_1by1, post_info);
}

static void
test_transfer_encoding_chunked_post (void)
{
  unsigned iter;
  for (iter = 0; iter < 4; iter++)
    {
      DskHttpClientStream *stream;
      DskHttpClientStreamOptions options = DSK_HTTP_CLIENT_STREAM_OPTIONS_DEFAULT;
      DskHttpRequestOptions req_options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
      DskHttpRequest *request;
      DskHttpClientStreamTransfer *xfer;
      DskHttpClientStreamFuncs request_funcs_0;
      DskError *error = NULL;
      RequestData request_data = REQUEST_DATA_DEFAULT;
      PostInfo post_info;
      dsk_warning ("iter=%u",iter);
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
      const char *post_data_str = "this is some POST data\n";
      req_options.verb = DSK_HTTP_VERB_POST;
      req_options.host = "localhost";
      req_options.full_path = "/hello.txt";
      request = dsk_http_request_new (&req_options, &error);
      DskMemorySource *post_data;
      post_data = dsk_memory_source_new ();
      fprintf (stderr, ".");
      if (iter == 0)
        {
          dsk_buffer_append_string (&post_data->buffer, post_data_str);
          dsk_memory_source_added_data (post_data);
          dsk_memory_source_done_adding (post_data);
        }
      xfer = dsk_http_client_stream_request (stream, request, DSK_OCTET_SOURCE (post_data),
                                             &request_funcs_0, &request_data);
      if (iter == 1)
        {
          dsk_buffer_append_string (&post_data->buffer, post_data_str);
          dsk_memory_source_added_data (post_data);
          dsk_memory_source_done_adding (post_data);
          dsk_object_unref (post_data);
        }
      else if (iter == 2)
        {
          post_info.str = post_data_str;
          post_info.memory_source = post_data;
          dsk_main_add_idle (write_post_data, &post_info);
        }
      else if (iter == 3)
        {
          post_info.str = post_data_str;
          post_info.memory_source = post_data;
          dsk_main_add_idle (write_post_data_1by1, &post_info);
        }

      /* read data from sink; pluck off POST Data */
      unsigned len;
      while (!is_http_request_complete (&request_data.sink->buffer, &len))
        dsk_main_run_once ();
      dsk_buffer_discard (&request_data.sink->buffer, len);
      const char *expected = "17\r\nthis is some POST data\n\r\n0\r\n\r\n\r\n";
      while (request_data.sink->buffer.size < strlen (expected))
        dsk_main_run_once ();
      dsk_assert (request_data.sink->buffer.size == strlen (expected));
      {
        char slab[1000];
        dsk_buffer_read (&request_data.sink->buffer, 1000, slab);
        dsk_assert (memcmp (slab, expected, strlen (expected)) == 0);
      }

      /* write response */
      static const char *content = 
                                "HTTP/1.1 200 OK\r\n"
                                "Date: Mon, 17 May 2010 22:50:08 GMT\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: 7\r\n"
                                "Connection: close\r\n"
                                "\r\n"
                                "hi mom\n";
      dsk_buffer_append_string (&request_data.source->buffer, content);
      dsk_memory_source_added_data (request_data.source);

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
}


static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple connection-close", test_simple_bigwrite },
  { "simple connection-close byte-by-byte", test_simple_bytebybyte },
  { "simple POST", test_simple_post },
  { "transfer-encoding chunked content", test_transfer_encoding_chunked },
  { "transfer-encoding chunked POST", test_transfer_encoding_chunked_post },
  // { "content-encoding gzip", test_content_encoding_gzip },
};

int main(void)
{
  unsigned i;
  for (i = 0; i < DSK_N_ELEMENTS (tests); i++)
    {
      fprintf (stderr, "Test: %s... ", tests[i].name);
      tests[i].test ();
      fprintf (stderr, " done.\n");
    }
  return 0;
}
