#include <string.h>
#include <stdio.h>
#include "../dsk.h"

typedef struct _RequestData RequestData;
struct _RequestData
{
  DskMemorySource *source;
  DskMemorySink *sink;
  DskHttpResponse *response_header;
  DskError *error;
  dsk_boolean content_complete;
  DskBuffer content;
  dsk_boolean destroyed;
};
static dsk_boolean cmdline_verbose = DSK_FALSE;
static dsk_boolean cmdline_print_error_messages = DSK_FALSE;
#define REQUEST_DATA_DEFAULT { NULL, NULL, NULL, NULL,      \
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
request_data__handle_error (DskHttpClientStreamTransfer *xfer)
{
  RequestData *rd = xfer->user_data;
  if (rd->error == NULL)
    rd->error = dsk_error_ref (xfer->owner->latest_error);
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
  if (rd->error)
    dsk_error_unref (rd->error);
  if (rd->response_header)
    dsk_object_unref (rd->response_header);
}

static dsk_boolean
is_http_request_complete (DskBuffer *buf,
                          unsigned  *length_out)
{
  char *slab = dsk_malloc (buf->size + 1);
  const char *a, *b;

  dsk_buffer_peek (buf, buf->size, slab);
  slab[buf->size] = 0;

  a = strstr (slab, "\n\n");
  b = strstr (slab, "\n\r\n");
  if (a == NULL && b == NULL)
    {
      dsk_free (slab);
      return DSK_FALSE;
    }
  if (length_out)
    {
      if (a == NULL)
        *length_out = b - slab + 3;
      else if (b == NULL)
        *length_out = a - slab + 2;
      else if (a < b)
        *length_out = a - slab + 2;
      else 
        *length_out = b - slab + 3;
    }

  dsk_free (slab);
  return DSK_TRUE;
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
  unsigned iter;
  for (iter = 0; iter < 3; iter++)
    {
      DskHttpClientStream *stream;
      DskHttpClientStreamOptions options = DSK_HTTP_CLIENT_STREAM_OPTIONS_DEFAULT;
      RequestData request_data = REQUEST_DATA_DEFAULT;
      DskHttpRequestOptions req_options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
      DskHttpRequest *request;
      DskHttpClientStreamTransfer *xfer;
      DskHttpClientStreamFuncs request_funcs_0;
      DskError *error = NULL;
      const char *content;
      switch (iter)
        {
        case 0:
          content = "HTTP/1.1 200 OK\r\n"
                    "Date: Mon, 17 May 2010 22:50:08 GMT\r\n"
                    "Content-Type: text/plain\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "7\r\nhi mom\n\r\n"
                    "0\r\n"
                    "\r\n";             /* no trailer */
          break;
        case 1:
          content = "HTTP/1.1 200 OK\r\n"
                    "Date: Mon, 17 May 2010 22:50:08 GMT\r\n"
                    "Content-Type: text/plain\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "7; random extension\r\nhi mom\n\r\n"
                    "0; another extension\r\n"
                    "\r\n";             /* no trailer */
          break;
        case 2:
          content = "HTTP/1.1 200 OK\r\n"
                    "Date: Mon, 17 May 2010 22:50:08 GMT\r\n"
                    "Content-Type: text/plain\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "7\r\nhi mom\n\r\n"
                    "0\r\n"
                    "X-Information: whatever\r\n"              /* trailer */
                    "\r\n";
          break;
        }
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
          fprintf (stderr, ".");
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
  if (*(post_info->str) == 0)
    {
      dsk_memory_source_done_adding (post_info->memory_source);
      dsk_object_unref (post_info->memory_source);
      return;
    }

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
      const char *expected = iter == 3
                   ? "1\r\nt\r\n" /* a bunch of 1-byte chunks */
                     "1\r\nh\r\n"
                     "1\r\ni\r\n"
                     "1\r\ns\r\n"
                     "1\r\n \r\n"
                     "1\r\ni\r\n"
                     "1\r\ns\r\n"
                     "1\r\n \r\n"
                     "1\r\ns\r\n"
                     "1\r\no\r\n"
                     "1\r\nm\r\n"
                     "1\r\ne\r\n"
                     "1\r\n \r\n"
                     "1\r\nP\r\n"
                     "1\r\nO\r\n"
                     "1\r\nS\r\n"
                     "1\r\nT\r\n"
                     "1\r\n \r\n"
                     "1\r\nd\r\n"
                     "1\r\na\r\n"
                     "1\r\nt\r\n"
                     "1\r\na\r\n"
                     "1\r\n\n\r\n"
                     "0\r\n\r\n"
                   : "17\r\nthis is some POST data\n\r\n0\r\n\r\n";
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

/* test keepalive under a variety of request and response types */
static void
test_keepalive_full (dsk_boolean add_postcontent_newlines)
{
  static const char *patterns[] = 
    /* Legend:  Query, get
                Post query
                Transfer-encoding chunked POST request
                Response, content-length
                Chunked response, transfer-encoding chunked */
    { "QRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQRQR",
      "QQRRQQRRQQRRQQRRQQRRQQRRQQRRQQRRQQRRQQRRQQRRQQRRQQRR",
      "QQQRRRQQQRRRQQQRRRQQQRRRQQQRRRQQQRRRQQQRRRQQQRRR",
      "QQQRCRQQQRCRQQQCRCQQQCCCQQQRRRQQQCCCQQQRCRQQQCCR" };
  RequestData request_data_default = REQUEST_DATA_DEFAULT;
  RequestData request_data_array[100]; /* max length of pattern string ! */
  DskHttpClientStreamFuncs request_funcs_0;
  unsigned iter;
  dsk_boolean debug = DSK_FALSE;
  memset (&request_funcs_0, 0, sizeof (request_funcs_0));
  request_funcs_0.handle_response = request_data__handle_response;
  request_funcs_0.handle_content_complete = request_data__handle_content_complete;
  request_funcs_0.destroy = request_data__destroy;
  for (iter = 0; iter < DSK_N_ELEMENTS (patterns); iter++)
    {
      const char *at = patterns[iter];
      unsigned n_requests = 0, n_responses = 0;
      DskHttpClientStream *stream;
      DskMemorySource *source;
      DskMemorySink *sink;
      DskHttpClientStreamOptions options = DSK_HTTP_CLIENT_STREAM_OPTIONS_DEFAULT;

      source = dsk_memory_source_new ();
      sink = dsk_memory_sink_new ();
      sink->max_buffer_size = 100000000;
      stream = dsk_http_client_stream_new (DSK_OCTET_SINK (sink),
                                           DSK_OCTET_SOURCE (source),
                                           &options);
      request_data_default.source = source;
      request_data_default.sink = sink;
      DskError *error = NULL;
      if (debug)
        fprintf (stderr, "[");
      else
        fprintf (stderr, ".");
      while (*at)
        {
          RequestData *rd;
          dsk_boolean is_query = (*at == 'Q' || *at == 'P' || *at == 'T');
          dsk_boolean is_response = (*at == 'R' || *at == 'C');
          dsk_assert (is_query || is_response);
          if (debug)
            fprintf(stderr, "%c", *at);
          if (is_query)
            {
              rd = request_data_array + n_requests;
              dsk_assert (n_requests < DSK_N_ELEMENTS (request_data_array));
              *rd = request_data_default;
              dsk_object_ref (rd->sink);
              dsk_object_ref (rd->source);      /* undone by clear() */
            }
          else
            {
              rd = request_data_array + n_responses;
              dsk_assert (n_responses < n_requests);
            }
          switch (*at)
            {
              /* === queries === */
            case 'Q':
              {
                DskHttpRequestOptions req_options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
                DskHttpRequest *request;
                DskHttpClientStreamTransfer *xfer;
                req_options.verb = DSK_HTTP_VERB_GET;
                req_options.host = "localhost";
                req_options.full_path = "/hello.txt";
                request = dsk_http_request_new (&req_options, &error);
                dsk_assert (request != NULL);
                xfer = dsk_http_client_stream_request (stream, request,
                                                       NULL,
                                                &request_funcs_0, rd);
                (void) xfer;            /* hmm */
              }
              break;
            case 'P':
              {
                DskHttpRequestOptions req_options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
                DskHttpRequest *request;
                DskHttpClientStreamTransfer *xfer;
                const char *pd_str = "this is post data\n";
                DskMemorySource *post_data;
                req_options.verb = DSK_HTTP_VERB_POST;
                req_options.host = "localhost";
                req_options.full_path = "/hello.txt";
                req_options.content_length = strlen (pd_str);
                request = dsk_http_request_new (&req_options, &error);
                dsk_assert (request != NULL);
                post_data = dsk_memory_source_new ();
                dsk_buffer_append_string (&post_data->buffer, pd_str);
                xfer = dsk_http_client_stream_request (stream, request,
                                                       DSK_OCTET_SOURCE (post_data),
                                                       &request_funcs_0, rd);
                dsk_memory_source_added_data (post_data);
                dsk_memory_source_done_adding (post_data);
                (void) xfer;            /* hmm */
              }
              break;
            case 'T':
              {
                DskHttpRequestOptions req_options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
                DskHttpRequest *request;
                DskHttpClientStreamTransfer *xfer;
                const char *pd_str = "this is post data\n";
                DskMemorySource *post_data;
                req_options.verb = DSK_HTTP_VERB_POST;
                req_options.host = "localhost";
                req_options.full_path = "/hello.txt";
                request = dsk_http_request_new (&req_options, &error);
                dsk_assert (request != NULL);
                post_data = dsk_memory_source_new ();
                dsk_buffer_append_string (&post_data->buffer, pd_str);
                xfer = dsk_http_client_stream_request (stream, request,
                                                       DSK_OCTET_SOURCE (post_data),
                                                       &request_funcs_0, rd);
                dsk_memory_source_added_data (post_data);
                dsk_memory_source_done_adding (post_data);
                (void) xfer;            /* hmm */
              }
              break;

              /* === responses === */
            case 'R':
              {
                char content_buf[8];
                /* write response to client stream */
                dsk_buffer_append_string (&source->buffer,
                                          "HTTP/1.1 200 Ok\r\n"
                                          "Content-Length: 8\r\n"
                                          "\r\n"
                                          "hi mom!\n"  /* 8 bytes */
                                         );
                if (add_postcontent_newlines)
                  dsk_buffer_append_string (&source->buffer, "\n");
                dsk_memory_source_added_data (source);

                /* handle response from DskHttpClientStream */
                while (!rd->destroyed)
                  dsk_main_run_once ();
                dsk_assert (rd->response_header != NULL);
                dsk_assert (rd->response_header->content_length == 8LL);
                dsk_assert (!rd->response_header->transfer_encoding_chunked);
                dsk_assert (rd->response_header->http_major_version == 1);
                dsk_assert (rd->response_header->http_minor_version == 1);
                dsk_assert (rd->response_header->status_code == 200);
                dsk_assert (rd->content_complete);
                dsk_assert (rd->content.size == 8);
                dsk_buffer_read (&rd->content, 8, content_buf);
                dsk_assert (memcmp (content_buf, "hi mom!\n", 8) == 0);
              }
              break;
            case 'C':
              {
                char content_buf[8];
                /* write response to client stream */
                dsk_buffer_append_string (&source->buffer,
                                          "HTTP/1.1 200 Ok\r\n"
                                          "Transfer-Encoding: chunked\r\n"
                                          "\r\n"
                                          "3\r\nhi \r\n"
                                          "5\r\nmom!\n\r\n"
                                          "0\r\n\r\n"
                                         );
                if (add_postcontent_newlines)
                  dsk_buffer_append_string (&source->buffer, "\r\n");
                dsk_memory_source_added_data (source);

                /* handle response from DskHttpClientStream */
                while (!rd->destroyed)
                  dsk_main_run_once ();
                dsk_assert (rd->response_header != NULL);
                dsk_assert (rd->response_header->content_length == -1LL);
                dsk_assert (rd->response_header->transfer_encoding_chunked);
                dsk_assert (rd->response_header->http_major_version == 1);
                dsk_assert (rd->response_header->http_minor_version == 1);
                dsk_assert (rd->response_header->status_code == 200);
                dsk_assert (rd->content_complete);
                dsk_assert (rd->content.size == 8);
                dsk_buffer_read (&rd->content, 8, content_buf);
                dsk_assert (memcmp (content_buf, "hi mom!\n", 8) == 0);
                
              }
              break;
            default:
              dsk_assert_not_reached ();
            }
          if (is_query)
            n_requests++;
          else
            {
              n_responses++;
              request_data_clear (rd);
            }
          at++;
        }
      if (debug)
        fprintf(stderr, "]");
      dsk_object_unref (stream);
      dsk_object_unref (source);
      dsk_object_unref (sink);
    }
}
static void
test_keepalive (void)
{
  test_keepalive_full (DSK_FALSE);
}
static void
test_keepalive_old_broken_clients (void)
{
  test_keepalive_full (DSK_TRUE);
}

static DskError *
test_bad_response (const char *response)
{
  DskHttpClientStream *stream;
  DskHttpClientStreamOptions options = DSK_HTTP_CLIENT_STREAM_OPTIONS_DEFAULT;
  RequestData request_data = REQUEST_DATA_DEFAULT;
  DskHttpRequestOptions req_options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
  DskHttpRequest *request;
  DskHttpClientStreamFuncs request_funcs_0;
  DskError *error = NULL;
  memset (&request_funcs_0, 0, sizeof (request_funcs_0));
  request_funcs_0.handle_response = request_data__handle_response;
  request_funcs_0.handle_content_complete = request_data__handle_content_complete;
  request_funcs_0.handle_error = request_data__handle_error;
  request_funcs_0.destroy = request_data__destroy;
  request_data.source = dsk_memory_source_new ();
  request_data.sink = dsk_memory_sink_new ();
  request_data.sink->max_buffer_size = 100000000;
  options.print_warnings = DSK_FALSE;
  stream = dsk_http_client_stream_new (DSK_OCTET_SINK (request_data.sink),
                                       DSK_OCTET_SOURCE (request_data.source),
                                       &options);
  req_options.full_path = "/hello.txt";
  request = dsk_http_request_new (&req_options, &error);
  dsk_assert (request != NULL);
  dsk_http_client_stream_request (stream, request,
                                  NULL,
                                  &request_funcs_0, &request_data);
  dsk_buffer_append_string (&request_data.source->buffer, response);
  dsk_memory_source_added_data (request_data.source);
  dsk_memory_source_done_adding (request_data.source);
  while (!request_data.destroyed)
    dsk_main_run_once ();
  dsk_assert (request_data.error != NULL);
  error = dsk_error_ref (request_data.error);
  request_data_clear (&request_data);
  return error;
}

static void
test_bad_responses (void)
{
  DskError *e;

  fprintf (stderr, ".");
  e = test_bad_response ("HTTP/1.1 qwerty\r\n"
                         "Content-Length: 7\r\n"
                         "\r\n"
                         "hi mom\n");
  if (cmdline_print_error_messages)
    fprintf (stderr, "bad nonnumeric status code error: %s\n", e->message);
  dsk_error_unref (e);

  fprintf (stderr, ".");
  e = test_bad_response ("HTTP/1.1 999\r\n"
                         "Content-Length: 7\r\n"
                         "\r\n"
                         "hi mom\n");
  if (cmdline_print_error_messages)
    fprintf (stderr, "bad numeric status code error: %s\n", e->message);
  dsk_error_unref (e);

  fprintf (stderr, ".");
  e = test_bad_response ("HTTP/1.1 200 Ok\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "\r\n"
                         "7x\r\nhi mom\n0\r\n\r\n");
  if (cmdline_print_error_messages)
    fprintf (stderr, "bad chunk header: %s\n", e->message);
  dsk_error_unref (e);

  fprintf (stderr, ".");
  e = test_bad_response ("HTTP/1.1 200 Ok\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "\r\n"
                         "7\r\nhi mom\n0x\r\n\r\n");
  if (cmdline_print_error_messages)
    fprintf (stderr, "bad final chunk header: %s\n", e->message);
  dsk_error_unref (e);
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
  { "pipelining and keepalive", test_keepalive },
  { "pipelining and keepalive, on old HTTP servers", test_keepalive_old_broken_clients },
  { "bad responses", test_bad_responses },
  // { "content-encoding gzip", test_content_encoding_gzip },
};

int main(int argc, char **argv)
{
  unsigned i;

  dsk_cmdline_init ("test HTTP server stream",
                    "Test the low-level HTTP server interface",
                    NULL, 0);
  dsk_cmdline_add_boolean ("verbose", "extra logging", NULL, 0,
                           &cmdline_verbose);
  dsk_cmdline_add_boolean ("print-error", "print error messages for bad-response tests", NULL, 0,
                           &cmdline_print_error_messages);
  dsk_cmdline_process_args (&argc, &argv);

  for (i = 0; i < DSK_N_ELEMENTS (tests); i++)
    {
      fprintf (stderr, "Test: %s... ", tests[i].name);
      tests[i].test ();
      fprintf (stderr, " done.\n");
    }
  return 0;
}