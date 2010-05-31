#include <string.h>
#include <stdio.h>
#include "../dsk.h"

static dsk_boolean set_boolean_true (void *obj, void *pbool)
{
  DSK_UNUSED (obj);
  * (dsk_boolean *) pbool = DSK_TRUE;
  return DSK_FALSE;
}

static unsigned bbb_rem;
static const uint8_t *bbb_at;
static void
add_byte_to_memory_source (void *data)
{
  DskMemorySource *csource = DSK_MEMORY_SOURCE (data);
  dsk_buffer_append_byte (&csource->buffer, *bbb_at++);
  bbb_rem--;
  if (bbb_rem > 0)
    dsk_main_add_idle (add_byte_to_memory_source, csource);
  dsk_memory_source_added_data (csource);
}

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
  unsigned header_i, iter, resp_iter;

  for (header_i = 0; header_i < DSK_N_ELEMENTS (headers); header_i++)
    for (iter = 0; iter < 2; iter++)
      for (resp_iter = 0; resp_iter < 3; resp_iter++)
        {
          const char *hdr = headers[header_i];
          unsigned hdr_len = strlen (hdr);
          DskError *error = NULL;
          DskMemorySource *csource = dsk_memory_source_new ();
          DskMemorySink *csink = dsk_memory_sink_new ();
          DskHttpServerStream *stream;
          DskHttpServerStreamOptions server_opts 
            = DSK_HTTP_SERVER_STREAM_OPTIONS_DEFAULT;
          DskHttpResponseOptions resp_opts
            = DSK_HTTP_RESPONSE_OPTIONS_DEFAULT;
          DskHttpServerStreamResponseOptions stream_resp_opts
            = DSK_HTTP_SERVER_STREAM_RESPONSE_OPTIONS_DEFAULT;
          csink->max_buffer_size = 128*1024;
          stream = dsk_http_server_stream_new (DSK_OCTET_SINK (csink),
                                               DSK_OCTET_SOURCE (csource),
                                               &server_opts);
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
          dsk_boolean got_notify;
          got_notify = DSK_FALSE;
          dsk_hook_trap (&stream->request_available,
                         set_boolean_true,
                         &got_notify,
                         NULL);
          while (!got_notify)
            dsk_main_run_once ();
          DskHttpServerStreamTransfer *xfer;
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

          DskMemorySource *content_source = NULL;

          /* send a response */
          switch (resp_iter)
            {
            case 0:
              stream_resp_opts.header_options = &resp_opts;
              stream_resp_opts.content_length = 7;
              stream_resp_opts.content_data = (const uint8_t *) "hi mom\n";
              break;
            case 1:
              /* streaming data, no content-length;
                 relies on Connection-Close */
              stream_resp_opts.header_options = &resp_opts;

              /* setting content_source will cause us to write the 
                 content in byte-by-byte below */
              content_source = dsk_memory_source_new ();
              stream_resp_opts.content_stream = DSK_OCTET_SOURCE (content_source);
              break;
            case 2:
              /* streaming data, no content-length;
                 relies on Connection-Close */
              stream_resp_opts.header_options = &resp_opts;
              stream_resp_opts.content_length = 7;

              /* setting content_source will cause us to write the 
                 content in byte-by-byte below */
              content_source = dsk_memory_source_new ();
              stream_resp_opts.content_stream = DSK_OCTET_SOURCE (content_source);
              break;
            }
          if (!dsk_http_server_stream_respond (xfer, &stream_resp_opts, &error))
            dsk_die ("error responding to request: %s", error->message);

          /* We retain content_source only so we can
             add the content byte-by-byte */
          if (content_source != NULL)
            {
              bbb_at = (const uint8_t *) "hi mom\n";
              bbb_rem = 7;
              dsk_main_add_idle (add_byte_to_memory_source, content_source);
              while (bbb_rem > 0)
                dsk_main_run_once ();
              dsk_memory_source_done_adding (content_source);
              dsk_object_unref (content_source);
              content_source = NULL;
            }

          /* read until EOF from sink */
          while (!csink->got_shutdown)
            dsk_main_run_once ();

          /* analyse response (header+body) */
          char *line;
          line = dsk_buffer_read_line (&csink->buffer);
          dsk_assert (line != NULL);
          dsk_assert (strncmp (line, "HTTP/1.", 7) == 0);
          dsk_assert (line[7] == '0' || line[7] == '1');
          dsk_assert (line[8] == ' ');
          dsk_assert (strncmp (line+9, "200", 3) == 0);
          dsk_assert (line[12] == 0 || dsk_ascii_isspace (line[12]));
          dsk_free (line);
          while ((line=dsk_buffer_read_line (&csink->buffer)) != NULL)
            {
              if (line[0] == '\r' || line[0] == 0)
                {
                  dsk_free (line);
                  break;
                }
              /* TODO: process header line? */
              dsk_free (line);
            }
          dsk_assert (line != NULL);
          line = dsk_buffer_read_line (&csink->buffer);
          dsk_assert (strcmp (line, "hi mom") == 0);
          dsk_free (line);
          dsk_assert (csink->buffer.size == 0);

          /* cleanup */
          dsk_object_unref (csource);
          dsk_object_unref (csink);
          dsk_object_unref (stream);
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
