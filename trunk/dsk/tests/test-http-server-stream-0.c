#include "../dsk.h"

static void
test_simple (void)
{
  static const char *headers[] =
    {
      "GET / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Connection: close\r\n"
      "\r\n"
      ,
      "GET / HTTP/1.0\r\n"
      "Host: localhost\r\n"
      "\r\n"
    };
  unsigned header_i, iter;

  for (header_i = 0; header_i < DSK_N_ELEMENTS (headers); header_i++)
    for (iter = 0; iter < 2; iter++)
      for (resp_iter = 0; resp_iter < 3; resp_iter++)
        {
          const char *hdr = headers[header_i];
          unsigned hdr_len = strlen (hdr);
          DskMemorySource *csource = dsk_memory_source_new ();
          DskMemorySink *csink = dsk_memory_sink_new ();
          DskHttpServerStream *stream;
          DskHttpServerStreamResponseOptions resp_opts
            = DSK_HTTP_SERVER_STREAM_RESPONSE_OPTIONS_DEFAULT;
          stream = dsk_http_server_stream_new (csink, csource);
          if (iter == 0)
            {
              /* Feed data to server in one bite */
              dsk_buffer_append (&csource->buffer, hdr_len, hdr);
              dsk_memory_source_added_data (csource);
              dsk_memory_source_done_adding (csource);
            }
          else
            {
              /* Feed data to server byte-by-byte */
              unsigned rem = hdr_len;
              const char *at = hdr;
              while (rem--)
                {
                  dsk_buffer_append_byte (&csource->buffer, *at++);
                  dsk_memory_source_added_data (csource);
                  while (csource->buffer.size > 0)
                    dsk_main_run_once ();
                }
              dsk_memory_source_done_adding (csource);
            }
          /* wait til we receive the request */
          got_notify = DSK_FALSE;
          dsk_hook_trap (&server->request_available,
                         set_boolean_true,
                         &got_notify,
                         NULL);
          while (!got_notify)
            dsk_main_run_once ();
          xfer = dsk_http_server_stream_get_request (stream);
          dsk_assert (xfer != NULL);
          dsk_assert (dsk_http_server_stream_get_request (stream) == NULL);

          switch (header_i)
            {
            case 0:
              dsk_assert (xfer->request->connection_close);
              dsk_assert (xfer->request->http_major_version == 1);
              dsk_assert (xfer->request->http_minor_version == 1);
              dsk_assert (!xfer->request->transfer_encoding_chunked);
              dsk_assert (xfer->request->content_length == -1LL);
              break;
            case 1:
              dsk_assert (xfer->request->http_major_version == 1);
              dsk_assert (xfer->request->http_minor_version == 0);
              dsk_assert (!xfer->request->transfer_encoding_chunked);
              dsk_assert (xfer->request->content_length == -1LL);
              break;
            }

          /* send a response */
          switch (resp_iter)
            {
            case 0:
              stream_resp_opts.header_options = &resp_opts;
              stream_resp_opts.content_length = 7;
              stream_resp_opts.content_data = "hi mom\n";
              break;
            case 1:
              stream_resp_opts.header_options = &resp_opts;
              stream_resp_opts.content_stream = dsk_memory_source_new ();
              dsk_main_add_idle (...);
              break;
            case 2:
              stream_resp_opts.header_options = &resp_opts;
              stream_resp_opts.content_length = 7;
              stream_resp_opts.content_stream = dsk_memory_source_new ();
              dsk_main_add_idle (...);
              break;
            }

          /* read until EOF from sink */
          ...

          /* analyse response (header+body) */
          ...
        }
}

static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple connection-close", test_simple },
  //{ "simple POST", test_simple_post },
  //{ "transfer-encoding chunked content", test_transfer_encoding_chunked },
  //{ "transfer-encoding chunked POST", test_transfer_encoding_chunked_post },
  //{ "content-encoding gzip", test_content_encoding_gzip },
};

int main(int argc, char **argv)
{
  unsigned i;

  dsk_cmdline_init ("test HTTP server stream",
                    "Test the low-level HTTP server interface",
                    NULL, 0);
  dsk_cmdline_process_args (&argc, &argv);

  for (i = 0; i < DSK_N_ELEMENTS (tests); i++)
    {
      fprintf (stderr, "Test: %s... ", tests[i].name);
      tests[i].test ();
      fprintf (stderr, " done.\n");
    }
  return 0;
}
