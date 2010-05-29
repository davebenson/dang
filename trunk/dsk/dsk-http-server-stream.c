#include "dsk.h"
#include "dsk-http-internals.h"
#include "../gsklistmacros.h"

#define GET_XFER_QUEUE(server_stream) \
  DskHttpServerStreamTransfer *,      \
  (server_stream)->first_transfer,    \
  (server_stream)->last_transfer,     \
  next

static void
handle_post_data_finalize (void *data)
{
  DskHttpServerStreamTransfer *xfer = data;
  dsk_assert (xfer->post_data != NULL);
  xfer->post_data = NULL;
}

static void
do_shutdown (DskHttpServerStream *ss,
             DskError            *e)
{
  DskError *error = e;
  while (ss->first_transfer != NULL)
    {
      DskHttpServerStreamTransfer *xfer = ss->first_transfer;
      ss->first_transfer = xfer->next;
      if (xfer->funcs != NULL)
        {
          if (xfer->funcs->error_notify != NULL)
            {
              if (error == NULL)
                error = dsk_error_new ("HTTP server shut down before transfer complete");
              xfer->funcs->error_notify (xfer, error);
            }
          if (xfer->funcs->destroy != NULL)
            xfer->funcs->destroy (xfer);
        }
      if (xfer->returned && !xfer->responded) /* 'else' is optimization: xfer->funcs => returned */
        {
          xfer->failed = DSK_TRUE;
          continue;                     /* do not continue destroying the xfer */
        }
      if (xfer->request)
        dsk_object_unref (xfer->request);
      if (xfer->response)
        dsk_object_unref (xfer->response);
      if (xfer->post_data)
        {
          dsk_object_untrap_finalize (DSK_OBJECT (xfer->post_data),
                                      handle_post_data_finalize,
                                      xfer);
          xfer->post_data = NULL;
        }
      dsk_free (xfer);
    }
  if (e == NULL && error != NULL)
    dsk_error_unref (error);
  dsk_hook_set_idle_notify (&ss->request_available, DSK_FALSE);
  ss->last_transfer = NULL;
  ss->read_transfer = NULL;
  ss->next_request = NULL;
  if (ss->read_trap)
    {
      dsk_hook_trap_destroy (ss->read_trap);
      ss->read_trap = NULL;
    }
  if (ss->write_trap)
    {
      dsk_hook_trap_destroy (ss->write_trap);
      ss->write_trap = NULL;
    }
  if (ss->source)
    {
      dsk_octet_source_shutdown (ss->source);
      dsk_object_unref (ss->source);
      ss->source = NULL;
    }
  if (ss->sink)
    {
      dsk_octet_sink_shutdown (ss->sink);
      dsk_object_unref (ss->sink);
      ss->sink = NULL;
    }
}

static void
server_set_error (DskHttpServerStream *server,
                  const char          *format,
                  ...)
{
  DskError *e;
  va_list args;
  va_start (args, format);
  e = dsk_error_new_valist (format, args);
  va_end (args);
  do_shutdown (server, e);
  dsk_error_unref (e);
}

static void
done_reading_post_data (DskHttpServerStream *ss)
{
  DskHttpServerStreamTransfer *xfer = ss->read_transfer;
  dsk_assert (xfer != NULL);
  dsk_assert (xfer->next == NULL);

  xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_DONE;
  if (xfer->funcs != NULL && xfer->funcs->post_data_complete != NULL)
    xfer->funcs->post_data_complete (xfer);
  if (ss->wait_for_content_complete 
   && ss->next_request == NULL)
    {
      ss->next_request = xfer;

      /* notify that we have a header */
      dsk_hook_set_idle_notify (&ss->request_available, DSK_TRUE);
    }
  ss->read_transfer = NULL;
}

static void
transfer_post_content (DskHttpServerStreamTransfer *xfer,
                       unsigned                     amount)
{
  if (xfer->post_data != NULL)
    {
      dsk_buffer_transfer (&xfer->post_data->buffer,
                           &xfer->owner->incoming_data,
                           amount);
      dsk_memory_source_added_data (xfer->post_data);
    }
  else
    dsk_buffer_discard (&xfer->owner->incoming_data, amount);
}

static dsk_boolean
handle_source_readable (DskOctetSource *source,
                      DskHttpServerStream *ss)
{
DskError *error = NULL;
DskHttpServerStreamTransfer *xfer;
switch (dsk_octet_source_read_buffer (source, &ss->incoming_data, &error))
  {
  case DSK_IO_RESULT_SUCCESS:
  case DSK_IO_RESULT_AGAIN:
    return DSK_TRUE;
  case DSK_IO_RESULT_EOF:
    do_shutdown (ss, NULL);
    return DSK_FALSE;
  case DSK_IO_RESULT_ERROR:
    do_shutdown (ss, error);
    dsk_error_unref (error);
    return DSK_FALSE;
  }
restart_processing:
if (ss->incoming_data.size == 0)
  return DSK_TRUE;
if (ss->read_transfer == NULL)
  {
    /* new transfer */
    xfer = dsk_malloc (sizeof (DskHttpServerStreamTransfer));
    xfer->owner = ss;
    xfer->request = NULL;
    xfer->response = NULL;
    xfer->post_data = NULL;
    xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_NEED_HEADER;
    xfer->read_info.need_header.checked = 0;
    xfer->returned = xfer->responded = 0;

    GSK_QUEUE_ENQUEUE (GET_XFER_QUEUE (ss), xfer);
    ss->read_transfer = xfer;
  }
else
  xfer = ss->read_transfer;
switch (xfer->read_state)
  {
  case DSK_HTTP_SERVER_STREAM_READ_NEED_HEADER:
    {
      unsigned start = xfer->read_info.need_header.checked;
      if (start == ss->incoming_data.size)
        break;

      if (!_dsk_http_scan_for_end_of_header (&ss->incoming_data, 
                                        &xfer->read_info.need_header.checked,
                                        DSK_FALSE))
        {
          /* check for max header size */
          if (xfer->read_info.need_header.checked > ss->max_header_size)
            {
              ss->read_trap = NULL;
              server_set_error (ss, "header too long (at least %u bytes)",
                                (unsigned)xfer->read_info.need_header.checked);
              return DSK_FALSE;
            }
          goto return_true;
        }
      else
        {
          /* parse header */
          unsigned header_len = xfer->read_info.need_header.checked;
          dsk_boolean has_content;
          DskHttpRequest *request;
          request = dsk_http_request_parse_buffer (&ss->incoming_data, header_len, &error);
          if (request == NULL)
            {
              server_set_error (ss, "HTTP server: error parsing HTTP request from client: %s",
                                error->message);
              return DSK_FALSE;
            }
          dsk_buffer_discard (&ss->incoming_data, header_len);
          has_content = request->verb == DSK_HTTP_VERB_PUT
                     || request->verb == DSK_HTTP_VERB_POST;

          if (has_content)
            {
              xfer->post_data = dsk_memory_source_new ();
              dsk_object_trap_finalize (DSK_OBJECT (xfer->post_data),
                                        handle_post_data_finalize,
                                        xfer);
            }

          dsk_boolean empty_content_body;
          empty_content_body = !has_content
                             || (request->content_length == 0LL
                                 && !request->transfer_encoding_chunked);

          if (empty_content_body)
            {
              xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_DONE;
              if (xfer->post_data != NULL)
                dsk_memory_source_done_adding (xfer->post_data);
              dsk_assert (xfer->next == NULL);
              ss->read_transfer = NULL;
            }
          else
            {
              if (request->transfer_encoding_chunked)
                {
                  xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNKED_HEADER;
                  xfer->read_info.in_xfer_chunk.remaining = 0;
                }
              else if (request->content_length != -1LL)
                {
                  xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_IN_POST;
                  xfer->read_info.in_post.remaining = request->content_length;
                }
              else
                {
                  server_set_error (ss, "need Content-Length or Transfer-Encoding chunked for %s data",
                                    dsk_http_verb_name (request->verb));
                  return DSK_FALSE;
                }
            }
          if (empty_content_body || !ss->wait_for_content_complete)
            {
              if (ss->next_request == NULL)
                ss->next_request = xfer;

              /* notify that we have a header */
              dsk_hook_set_idle_notify (&ss->request_available, DSK_TRUE);
            }
        }
      goto restart_processing;
    }
  case DSK_HTTP_SERVER_STREAM_READ_IN_POST:
    {
      unsigned amount = ss->incoming_data.size;
      if (amount > xfer->read_info.in_post.remaining)
        amount = xfer->read_info.in_post.remaining;

      if (xfer->post_data != NULL)
        dsk_buffer_transfer (&xfer->post_data->buffer, &ss->incoming_data, amount);
      else
        dsk_buffer_discard (&ss->incoming_data, amount);
      xfer->read_info.in_post.remaining -= amount;
      if (xfer->read_info.in_post.remaining == 0)
        {
          /* done reading post data */
          done_reading_post_data (ss);
          goto restart_processing;
        }
      goto return_true;
    }
  case DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNKED_HEADER:
  in_xfer_chunk_header:
    {
      int c;
      while ((c=dsk_buffer_read_byte (&ss->incoming_data)) != -1)
        {
          if (dsk_ascii_isxdigit (c))
            {
              xfer->read_info.in_xfer_chunk.remaining <<= 4;
              xfer->read_info.in_xfer_chunk.remaining += dsk_ascii_xdigit_value (c);
            }
          else if (c == '\n')
            {
              /* switch state */
              if (xfer->read_info.in_xfer_chunk.remaining == 0)
                {
                  xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_XFER_CHUNK_TRAILER;
                  goto in_trailer;
                }
              else
                {
                  xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNK;
                  goto in_xfer_chunk;
                }
            }
          else if (dsk_ascii_isspace (c))
            {
              /* do nothing */
            }
          else if (c == ';')
            {
              xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNKED_HEADER_EXTENSION;
              goto in_xfer_chunked_header_extension;
            }
          else
            {
              server_set_error (ss, "unexpected character %s in chunked POST data",
                                dsk_ascii_byte_name (c));
              return DSK_FALSE;
            }
        }
      goto return_true;
    }
  case DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNKED_HEADER_EXTENSION:
  in_xfer_chunked_header_extension:
    {
      int c;
      while ((c=dsk_buffer_read_byte (&ss->incoming_data)) != -1)
        {
          if (c == '\n')
            {
              /* switch state */
              xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNK;
              goto in_xfer_chunk;
            }
          else
            {
              /* do nothing */
            }
        }
      goto return_true;
    }
  case DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNK:
  in_xfer_chunk:
    {
      unsigned amount = ss->incoming_data.size;
      if (amount > xfer->read_info.in_xfer_chunk.remaining)
        amount = xfer->read_info.in_xfer_chunk.remaining;
      transfer_post_content (xfer, amount);
      xfer->read_info.in_xfer_chunk.remaining -= amount;
      if (xfer->read_info.in_xfer_chunk.remaining == 0)
        {
          xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_AFTER_XFER_CHUNK;
          goto after_xfer_chunk;
        }
      goto return_true;
    }
  case DSK_HTTP_SERVER_STREAM_READ_AFTER_XFER_CHUNK:
  after_xfer_chunk:
    {
      for (;;)
        {
          int c = dsk_buffer_read_byte (&ss->incoming_data);
          if (c == '\r')
            continue;
          if (c == '\n')
            {
              xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNKED_HEADER;
              xfer->read_info.in_xfer_chunk.remaining = 0;
              goto in_xfer_chunk_header;
            }
          if (c == -1)
            goto return_true;
          server_set_error (ss,
                            "unexpected char %s after chunk",
                            dsk_ascii_byte_name (c));
          return DSK_FALSE;
        }
    }
    break;
  case DSK_HTTP_SERVER_STREAM_READ_XFER_CHUNK_TRAILER:
  in_trailer:
    /* We are looking for the end of content similar to the end
       of an http-header: two consecutive newlines. */
    if (!_dsk_http_scan_for_end_of_header (&ss->incoming_data, 
                            &xfer->read_info.in_xfer_chunk_trailer.checked,
                                           DSK_TRUE))
      {
        goto return_true;
      }
    dsk_buffer_discard (&ss->incoming_data,
                        xfer->read_info.in_xfer_chunk_trailer.checked);
    xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_XFER_CHUNK_FINAL_NEWLINE;
    /* fallthrough */
  case DSK_HTTP_SERVER_STREAM_READ_XFER_CHUNK_FINAL_NEWLINE:
    {
      int c;
      while ((c = dsk_buffer_peek_byte (&ss->incoming_data)) != -1)
        {
          if (c == '\r' || c == ' ')
            {
              dsk_buffer_discard (&ss->incoming_data, 1);
              continue;
            }
          if (c == '\n')
            {
              dsk_buffer_discard (&ss->incoming_data, 1);
              done_reading_post_data (ss);
              xfer = NULL;            /* reduce chances of bugs */
              goto restart_processing;
            }
          //if (!ss->strict_keepalive)
            {
              done_reading_post_data (ss);
              xfer = NULL;            /* reduce chances of bugs */
              goto restart_processing;
            }
          //else
            //{
              //server_set_error (ss,
                                //"bad character after transfer-encoding: chunked; expected newline");
              //return DSK_FALSE;
            //}
        }
      goto return_true;
    }
  case DSK_HTTP_SERVER_STREAM_READ_DONE:
    dsk_assert_not_reached ();
    break;
  }

return_true:
  /* In the client, we handled EOF at the end;
     such behavior isn't really appropriate for the server.
     (And I'm not sure it makes sense for the client either) */
  return DSK_TRUE;
}

DskHttpServerStream *
dsk_http_server_stream_new     (DskOctetSink        *sink,
                              DskOctetSource      *source)
{
  DskHttpServerStream *ss = dsk_object_new (&dsk_http_server_stream_class);
  ss->sink = dsk_object_ref (sink);
  ss->source = dsk_object_ref (source);
  ss->read_trap = dsk_hook_trap (&source->readable_hook,
                                 (DskHookFunc) handle_source_readable,
                                 ss, NULL);
  return ss;
}

static dsk_boolean
is_ready_to_return (DskHttpServerStream *stream,
                    DskHttpServerStreamTransfer *xfer)
{
  if (xfer->request == NULL)
    return DSK_FALSE;
  if (xfer->read_state == DSK_HTTP_SERVER_STREAM_READ_DONE)
    return DSK_TRUE;
  if (!stream->wait_for_content_complete)
    return DSK_TRUE;
  return DSK_FALSE;
}

DskHttpServerStreamTransfer *
dsk_http_server_stream_get_request (DskHttpServerStream *stream)
{
  DskHttpServerStreamTransfer *rv = stream->next_request;
  if (rv != NULL)
    {
      rv->returned = DSK_TRUE;
      stream->next_request = rv->next;
      if (!is_ready_to_return (stream, stream->next_request))
          stream->next_request = NULL;

        /* If the ref-count of the post-data gets to 0,
           we will see the finalizer handler get called,
           which will in turn set post_data to NULL;
           once post_data is NULL, we discard any further POST data. */
        if (rv->post_data != NULL)
          dsk_main_add_idle (dsk_object_unref_f, rv->post_data);
      }
    if (stream->next_request == NULL)
      dsk_hook_set_idle_notify (&stream->request_available, DSK_FALSE);
    return rv;
}

void
dsk_http_server_stream_set_funcs (DskHttpServerStreamTransfer      *transfer,
                                  DskHttpServerStreamFuncs         *funcs,
                                  void                             *func_data)
{
  transfer->funcs = funcs;
  transfer->func_data = func_data;
}

static dsk_boolean
check_header       (DskHttpServerStreamTransfer *transfer,
                    DskHttpServerStreamResponseOptions *options,
                    DskError                   **error)
{
  DskHttpResponse *header = options->header;
  DSK_UNUSED (transfer);
  if (options->content_stream != NULL)
    {
      int64_t len = dsk_octet_source_get_length (options->content_stream);
      if (len >= 0LL)
        {
          if (options->content_length >= 0LL && len != options->content_length)
            {
              dsk_set_error (error, "Content-Length mismatch (stream v response options)");
              return DSK_FALSE;
            }
          if (header->content_length >= 0LL && len != header->content_length)
            {
              dsk_set_error (error, "Content-Length mismatch (stream v header)");
              return DSK_FALSE;
            }
          header->content_length = len;
        }
    }
  if (options->content_length >= 0)
    {
      if (header->content_length >= 0LL && options->content_length != header->content_length)
        {
          dsk_set_error (error, "Content-Length mismatch (response options v header)");
          return DSK_FALSE;
        }
      header->content_length = options->content_length;
    }
  return DSK_TRUE;
}

static DskHttpResponse *
construct_response (DskHttpServerStreamTransfer *transfer,
                    DskHttpServerStreamResponseOptions *options,
                    DskError **error)
{
  DskHttpResponseOptions hdr = *(options->header_options);
  DskHttpResponse *rv;
  if (options->content_stream != NULL)
    {
      int64_t len = dsk_octet_source_get_length (options->content_stream);
      if (len >= 0LL)
        {
          if (options->content_length >= 0LL && len != options->content_length)
            {
              dsk_set_error (error, "Content-Length mismatch (stream v response options)");
              return NULL;
            }
          if (hdr.content_length >= 0LL && len != hdr.content_length)
            {
              dsk_set_error (error, "Content-Length mismatch (stream v header options)");
              return NULL;
            }
          hdr.content_length = len;
        }
    }
  if (options->content_length >= 0)
    {
      if (hdr.content_length >= 0LL && options->content_length != hdr.content_length)
        {
          dsk_set_error (error, "Content-Length mismatch (response options v header options)");
          return NULL;
        }
      hdr.content_length = options->content_length;
    }
  if (hdr.request == NULL)
    hdr.request = transfer->request;
  rv = dsk_http_response_new (&hdr, error);
  /* cleanup? */
  return rv;
}

dsk_boolean
dsk_http_server_stream_respond (DskHttpServerStreamTransfer *transfer,
                                DskHttpServerStreamResponseOptions *options,
                                DskError **error)
{
  dsk_assert (transfer->returned);
  dsk_assert (!transfer->responded);

  transfer->responded = DSK_TRUE;

  /* transfer has failed (transport error or protocol error);
     silently destroy it; options is allowed to be NULL in this case. */
  if (transfer->failed)
    {
      dsk_object_unref (transfer->request);
      if (transfer->post_data)
        {
          dsk_object_untrap_finalize (DSK_OBJECT (transfer->post_data),
                                      handle_post_data_finalize,
                                      transfer);
          dsk_object_unref (transfer->post_data);
          transfer->post_data = NULL;
        }
      dsk_free (transfer);
      if (options != NULL)
        {
          dsk_set_error (error, "transfer has failed before you responded");
          return DSK_FALSE;
        }
      return DSK_TRUE;
    }
  DskHttpResponse *header;
  if (options->header != NULL)
    {
      if (!check_header (transfer, options, error))
        goto invalid_arguments;
      header = dsk_object_ref (options->header);
    }
  else if (options->header_options != NULL)
    {
      header = construct_response (transfer, options, error);
      if (header == NULL)
        goto invalid_arguments;
    }
  else
    goto invalid_arguments;
  if (dsk_http_has_response_body (transfer->request->verb, header->status_code))
    {
      if (options->content_stream != NULL)
        {
          ...
        }
      else if (header->content_length == 0)
        {
          ...
        }
      else if (header->content_data != NULL)
        {
          ...
        }
      else
        {
          error
        }
      ...
    }
  else if (options->content_stream != NULL)
    {
      /* what to do: ignore or drain or error? */
      ...
    }

  transfer->response = header;

  if (transfer->owner->first_transfer == transfer)
    {
      /* shove header into outgoing buffer */
      ...

      /* trap content readable */
      ...
    }
  else
    {
      /* waiting for another transfer to finish writing */
    }
  return DSK_TRUE;

invalid_arguments:
  /* internal server error ?? */
  ...
}

