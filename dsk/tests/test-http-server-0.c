#include <stdio.h>
#include <string.h>
#include "../dsk.h"

static dsk_boolean cmdline_verbose = DSK_FALSE;

static void
respond_text_string (DskHttpServerRequest *request,
                     void                 *func_data)
{
  DskHttpServerResponseOptions response_options = DSK_HTTP_SERVER_RESPONSE_OPTIONS_DEFAULT;
  const char *str = (const char *) func_data;
  unsigned length = strlen (str);
  DskMemorySource *source = dsk_memory_source_new ();
  dsk_buffer_append (&source->buffer, length, func_data);
  dsk_memory_source_added_data (source);
  dsk_memory_source_done_adding (source);
  response_options.content_length = length;             /* optional */
  dsk_http_server_request_respond (request, &response_options);
}

typedef struct _SimpleRequestInfo SimpleRequestInfo;
struct _SimpleRequestInfo
{
  dsk_boolean done;
  dsk_boolean failed;
  DskBuffer content_buffer;
  DskHttpStatus status_code;
};
#define SIMPLE_REQUEST_INFO_INIT { DSK_FALSE, DSK_FALSE, DSK_BUFFER_STATIC_INIT, 0 }

static void simple__handle_response (DskHttpClientStreamTransfer *xfer)
{
  SimpleRequestInfo *sri = xfer->user_data;
  sri->status_code = xfer->response->status_code;
}
static void simple__handle_content_complete (DskHttpClientStreamTransfer *xfer)
{
  DSK_UNUSED (xfer);
}
static void simple__handle_error (DskHttpClientStreamTransfer *xfer)
{
  SimpleRequestInfo *sri = xfer->user_data;
  sri->failed = DSK_TRUE;
}
static void simple__handle_destroy (DskHttpClientStreamTransfer *xfer)
{
  SimpleRequestInfo *sri = xfer->user_data;
  sri->done = DSK_TRUE;
}

static DskHttpClientStreamFuncs simple_client_stream_funcs =
{
  simple__handle_response,
  simple__handle_content_complete,
  simple__handle_error,
  simple__handle_destroy
};
static void
test_simple_http_server (void)
{
  DskHttpServer *server = dsk_http_server_new ();
  DskError *error = NULL;

  dsk_http_server_match_save (server);
  dsk_http_server_add_match (server, DSK_HTTP_SERVER_MATCH_PATH, "/hello.txt");
  dsk_http_server_register_cgi_handler (server,
                                        respond_text_string,
                                        "hello\n",
                                        NULL);
  dsk_http_server_match_restore (server);


  if (!dsk_http_server_bind_local (server, "tests/sockets/http-server.socket",
                                   &error))
    dsk_die ("error binding: %s", error->message);

  /* use a client-stream to fetch hello.txt */
  DskClientStreamOptions cs_options = DSK_CLIENT_STREAM_OPTIONS_DEFAULT;
  DskHttpClientStreamOptions http_client_stream_options = DSK_HTTP_CLIENT_STREAM_OPTIONS_DEFAULT;
  DskHttpClientStreamRequestOptions request_options = DSK_HTTP_CLIENT_STREAM_REQUEST_OPTIONS_DEFAULT;
  DskHttpRequestOptions req_options = DSK_HTTP_REQUEST_OPTIONS_DEFAULT;
  request_options.request_options = &req_options;
  cs_options.path = "tests/sockets/http-server.socket";
  DskOctetSink *client_sink;
  DskOctetSource *client_source;
  if (!dsk_client_stream_new (&cs_options, NULL, &client_sink, &client_source,
                              &error))
    dsk_die ("error creating client-stream");
  DskHttpClientStream *http_client_stream;
  http_client_stream = dsk_http_client_stream_new (client_sink, client_source,
                                                   &http_client_stream_options);
  DskHttpClientStreamTransfer *xfer;
  {
  SimpleRequestInfo sri = SIMPLE_REQUEST_INFO_INIT;
  req_options.path = "/hello.txt";
  request_options.funcs = &simple_client_stream_funcs;
  request_options.user_data = &sri;
  xfer = dsk_http_client_stream_request (http_client_stream,
                                         &request_options, &error);
  if (xfer == NULL)
    dsk_die ("error creating client-stream request: %s", error->message);
  while (!sri.done)
    dsk_main_run_once ();
  dsk_assert (sri.content_buffer.size == 6);
  char content_buf[6];
  dsk_buffer_read (&sri.content_buffer, 6, content_buf);
  dsk_assert (memcmp (content_buf, "hello\n", 6) == 0);
  dsk_assert (sri.status_code == 200);
  dsk_assert (!sri.failed);
  }

  /* use a client-stream to fetch anything else to get a 404 */
  {
  SimpleRequestInfo sri = SIMPLE_REQUEST_INFO_INIT;
  req_options.path = "/does-not-exist";
  request_options.funcs = &simple_client_stream_funcs;
  request_options.user_data = &sri;
  xfer = dsk_http_client_stream_request (http_client_stream,
                                         &request_options, &error);
  if (xfer == NULL)
    dsk_die ("error creating client-stream request: %s", error->message);
  while (!sri.done)
    dsk_main_run_once ();
  dsk_assert (sri.status_code == 404);
  dsk_assert (!sri.failed);
  }

  dsk_object_unref (http_client_stream);
  dsk_object_unref (server);
}

static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple http server", test_simple_http_server },
};

int main(int argc, char **argv)
{
  unsigned i;

  dsk_cmdline_init ("test HTTP server",
                    "Test the high-level HTTP server interface",
                    NULL, 0);
  dsk_cmdline_add_boolean ("verbose", "extra logging", NULL, 0,
                           &cmdline_verbose);
  dsk_cmdline_process_args (&argc, &argv);

  for (i = 0; i < DSK_N_ELEMENTS (tests); i++)
    {
      fprintf (stderr, "Test: %s... ", tests[i].name);
      tests[i].test ();
      fprintf (stderr, " done.\n");
    }
  dsk_cleanup ();
  return 0;
}
