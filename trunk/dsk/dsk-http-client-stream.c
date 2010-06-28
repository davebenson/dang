#include "dsk.h"
#include "dsk-http-internals.h"
#include "../gsklistmacros.h"

#define GET_STREAM_XFER_QUEUE(stream) \
        DskHttpClientStreamTransfer *, \
        stream->first_transfer, stream->last_transfer, \
        next

#define MAX_XFER_CHUNKED_EXTENSION_LENGTH               512


/* 16-hex digits + CRLF */
#define MAX_CHUNK_HEADER_SIZE                   18

static void
client_stream_set_error (DskHttpClientStream *stream,
                         DskHttpClientStreamTransfer *xfer,
                         const char          *format,
                         ...)
{
  va_list args;
  DskError *error;

  va_start (args, format);
  error = dsk_error_new_valist (format, args);
  va_end (args);
  if (stream->print_warnings)
    dsk_warning ("client_stream_set_error: %s [xfer=%p]",
                 error->message, xfer);

  /* set latest error */
  if (stream->latest_error)
    dsk_error_unref (stream->latest_error);
  stream->latest_error = error;

  /* notifications */
  if (xfer != NULL)
    {
      xfer->failed = DSK_TRUE;
      if (xfer != NULL && xfer->funcs->handle_error != NULL)
        xfer->funcs->handle_error (xfer);
    }
  dsk_hook_notify (&stream->error_hook);
}

static void
free_transfer (DskHttpClientStreamTransfer *xfer)
{
  dsk_object_unref (xfer->request);
  if (xfer->response)
    dsk_object_unref (xfer->response);
  if (xfer->content)
    {
      dsk_memory_source_done_adding (xfer->content);
      dsk_object_unref (xfer->content);
    }
  if (xfer->funcs != NULL && xfer->funcs->destroy != NULL)
    xfer->funcs->destroy (xfer);
  if (xfer->write_state == DSK_HTTP_CLIENT_STREAM_WRITE_CONTENT
   && xfer->write_info.in_content.post_data_trap != NULL)
    {
      dsk_hook_trap_destroy (xfer->write_info.in_content.post_data_trap);
      xfer->write_info.in_content.post_data_trap = NULL;
    }
  if (xfer->post_data)
    {
      dsk_octet_source_shutdown (xfer->post_data);
      dsk_object_unref (xfer->post_data);
    }
  if (xfer->content_decoder)
    dsk_object_unref (xfer->content_decoder);
  dsk_free (xfer);
}

static inline void
transfer_done (DskHttpClientStreamTransfer *xfer)
{
  /* remove from list */
  DskHttpClientStreamTransfer *tmp;
  GSK_QUEUE_DEQUEUE (GET_STREAM_XFER_QUEUE (xfer->owner), tmp);
  dsk_assert (tmp == xfer);

  if (xfer->content_decoder != NULL)
    {
      DskError *error = NULL;
      if (!dsk_octet_filter_finish (xfer->content_decoder,
                                    &xfer->content->buffer,
                                    &error))
        {
          client_stream_set_error (xfer->owner, xfer,
                                   "http-client: error decompressing content: %s",
                                   error->message);
          dsk_error_unref (error);
          return;
        }
      dsk_memory_source_added_data (xfer->content);
    }

  /* EOF for content */
  if (xfer->content)
    dsk_memory_source_done_adding (xfer->content);

  if (xfer->funcs->handle_content_complete != NULL)
    xfer->funcs->handle_content_complete (xfer);
  if (xfer->funcs->destroy != NULL)
    xfer->funcs->destroy (xfer);

  if (xfer->content)
    dsk_object_unref (xfer->content);
  dsk_object_unref (xfer->request);
  dsk_object_unref (xfer->response);
  if (xfer->content_decoder)
    dsk_object_unref (xfer->content_decoder);
  dsk_free (xfer);
}

static void
do_shutdown (DskHttpClientStream *stream)
{
  /* shutdown both sides */
  if (stream->read_trap != NULL)
    {
      dsk_hook_trap_destroy (stream->read_trap);
      stream->read_trap = NULL;
    }
  if (stream->write_trap != NULL)
    {
      dsk_hook_trap_destroy (stream->write_trap);
      stream->write_trap = NULL;
    }
  if (stream->source != NULL)
    {
      DskOctetSource *source = stream->source;
      stream->source = NULL;
      dsk_octet_source_shutdown (source);
      dsk_object_unref (source);
    }
  if (stream->sink != NULL)
    {
      DskOctetSink *sink = stream->sink;
      stream->sink = NULL;
      dsk_octet_sink_shutdown (sink);
      dsk_object_unref (sink);
    }

  /* destroy all pending transfers */
  DskHttpClientStreamTransfer *xfer_list = stream->first_transfer;
  dsk_boolean got_cancel = DSK_FALSE;
  stream->first_transfer = stream->last_transfer = NULL;
  stream->outgoing_data_transfer = NULL;
  while (xfer_list != NULL)
    {
      DskHttpClientStreamTransfer *next = xfer_list->next;
      if (!xfer_list->failed
       && xfer_list->read_state != DSK_HTTP_CLIENT_STREAM_READ_IN_BODY_EOF)
        {
          if (!got_cancel)
            {
              client_stream_set_error (stream, NULL,
                                       "EOF while request pending");
              got_cancel = DSK_TRUE;
            }
          if (xfer_list->funcs->handle_error != NULL)
            xfer_list->funcs->handle_error (xfer_list);
        }
      free_transfer (xfer_list);
      xfer_list = next;
    }
}

static dsk_boolean
has_response_body (DskHttpRequest *request,
                   DskHttpResponse *response)
{
  /* TODO: check all verbs */
  DSK_UNUSED (response);
  return (request->verb != DSK_HTTP_VERB_HEAD);
}


static unsigned
get_chunked_header (size_t size,
                    char  *chunked_header)
{
  static const char hex_digit[] = "0123456789abcdef";
  unsigned at = 0, i;
  size_t s = size;
  while (s)
    {
      chunked_header[at++] = hex_digit[s & 0x0f];
      s >>= 4;
    }
  for (i = 0; i < at / 2; i++)
    {
      char swap = chunked_header[i];
      chunked_header[i] = chunked_header[at - 1 - i];
      chunked_header[at - 1 - i] = swap;
    }
  chunked_header[at++] = '\r';
  chunked_header[at++] = '\n';
  return at;
}

static void
transfer_content (DskHttpClientStreamTransfer *xfer,
                  unsigned                     amount)
{
  /* TODO: handle compressing */
  if (xfer->content_decoder != NULL)
    {
      DskError *error = NULL;
      if (!dsk_octet_filter_process_buffer (xfer->content_decoder,
                                            &xfer->content->buffer,
                                            amount, &xfer->owner->incoming_data,
                                            DSK_TRUE, &error))
        {
          client_stream_set_error (xfer->owner, xfer,
                                   "http-client: error decompressing content: %s",
                                   error->message);
          dsk_error_unref (error);
          return;
        }
    }
  else
    dsk_buffer_transfer (&xfer->content->buffer,
                         &xfer->owner->incoming_data,
                         amount);
  dsk_memory_source_added_data (xfer->content);
}

static dsk_boolean
handle_transport_source_readable (DskOctetSource *source,
                                  void           *data)
{
  DskHttpClientStream *stream = data;
  DskError *error = NULL;
  DskHttpResponse *response;
  dsk_boolean got_eof = DSK_FALSE;

  switch (dsk_octet_source_read_buffer (source, &stream->incoming_data, &error))
    {
    case DSK_IO_RESULT_SUCCESS:
      break;
    case DSK_IO_RESULT_AGAIN:
      return DSK_TRUE;
    case DSK_IO_RESULT_ERROR:
      client_stream_set_error (stream,
                               stream->first_transfer,
                               "error reading from underlying transport: %s",
                               error->message);
      dsk_error_unref (error);
      return DSK_TRUE;
    case DSK_IO_RESULT_EOF:
      got_eof = DSK_TRUE;
      break;
    }
restart_processing:
  while (stream->incoming_data.size > 0)
    {
      DskHttpClientStreamTransfer *xfer = stream->first_transfer;
      if (xfer == NULL)
        {
          /* Try and ignore any spurious blanklines that precede the header */
          int c;
          c = dsk_buffer_peek_byte (&stream->incoming_data);
          if (c == '\r' || c == '\n')
            {
              dsk_buffer_discard (&stream->incoming_data, 1);
              continue;
            }
          stream->read_trap = NULL;
          client_stream_set_error (stream, xfer, "got data when none expected");
          do_shutdown (stream);
          return DSK_FALSE;
        }
      switch (xfer->read_state)
        {
          /* The key pattern we are looking for is:
                \n \r? \n
           */
        case DSK_HTTP_CLIENT_STREAM_READ_NEED_HEADER:
          {
            unsigned start = xfer->read_info.need_header.checked;
            char peek_buf[2];
            int peek_rv;
            if (start == stream->incoming_data.size)
              break;
            if (start == 0)
              {
                /* Ignore unexpected newlines: recommended by RFC 2616
                   for old (now super-ancient) clients */
                for (;;)
                  {
                    peek_rv = dsk_buffer_peek (&stream->incoming_data, 2, peek_buf);
                    if (peek_rv >= 1 && peek_buf[0] == '\n')
                      {
                        dsk_buffer_discard (&stream->incoming_data, 1);
                        continue;
                      }
                    if (peek_rv == 2 && peek_buf[0] == '\r' && peek_buf[1] == '\n')
                      {
                        dsk_buffer_discard (&stream->incoming_data, 2);
                        continue;
                      }
                    break;
                  }
              }

            if (!_dsk_http_scan_for_end_of_header (&stream->incoming_data, 
                                              &xfer->read_info.need_header.checked,
                                              DSK_FALSE))
              {
                /* check for max header size */
                if (xfer->read_info.need_header.checked > stream->max_header_size)
                  {
                    stream->read_trap = NULL;
                    client_stream_set_error (stream, xfer,
                                             "header too long (at least %u bytes)",
                                             (unsigned)xfer->read_info.need_header.checked);
                    do_shutdown (stream);
                    return DSK_FALSE;
                  }
                goto return_true;
              }
            /* parse header */
            unsigned header_len = xfer->read_info.need_header.checked;
            response = dsk_http_response_parse_buffer (&stream->incoming_data, header_len, &error);
            dsk_buffer_discard (&stream->incoming_data, header_len);
            if (response == NULL)
              {
                stream->read_trap = NULL;
                client_stream_set_error (stream, xfer,
                                         "parsing response header: %s",
                                         error->message);
                do_shutdown (stream);
                dsk_error_unref (error);
                return DSK_FALSE;
              }
            if (response->status_code == DSK_HTTP_STATUS_CONTINUE)
              {
                /* Someday we may want to give the user an opportunity to examine
                   the continue header, or to know that it happened, but we don't. */
                dsk_object_unref (response);
                goto restart_processing;
              }
            xfer->response = response;
            if (has_response_body (xfer->request, xfer->response))
              {
                /* setup (empty) response stream */
                xfer->content = dsk_memory_source_new ();

                if (xfer->response->transfer_encoding_chunked)
                  {
                    xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNKED_HEADER;
                    xfer->read_info.in_xfer_chunk.remaining = 0;
                  }
                else if (xfer->response->content_length > 0)
                  {
                    xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_IN_BODY;
                    xfer->read_info.in_body.remaining = xfer->response->content_length;
                  }
                else if (xfer->response->content_length == 0)
                  {
                    xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_DONE;
                    /* TODO: indicate source done */
                  }
                else
                  {
                    if (!xfer->response->connection_close)
                      {
                        client_stream_set_error (stream, xfer,
                                                 "neither 'Connection: close' nor 'Transfer-Encoding: chunked' nor 'Content-Length' specified");
                        xfer->response->connection_close = 1;   /* HACK */
                      }
                    xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_IN_BODY_EOF;
                  }
                if (xfer->uncompress_content
                 && xfer->response->content_encoding_gzip)
                  {
                    xfer->content_decoder = dsk_zlib_decompressor_new (DSK_ZLIB_GZIP);
                  }
              }
            else
              {
                xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_DONE;
              }
            /* notify */
            if (xfer->funcs->handle_response != NULL)
              xfer->funcs->handle_response (xfer);
            if (xfer->read_state == DSK_HTTP_CLIENT_STREAM_READ_DONE)
              transfer_done (xfer);             /* frees transfer */
          }
          break;
        case DSK_HTTP_CLIENT_STREAM_READ_IN_BODY:
          {
            unsigned amount = xfer->read_info.in_body.remaining;
            size_t old_size;
            if (amount > stream->incoming_data.size)
              amount = stream->incoming_data.size;
            xfer->read_info.in_body.remaining -= amount;
            old_size = xfer->content->buffer.size;
            transfer_content (xfer, amount);
            dsk_memory_source_added_data (xfer->content);
            if (xfer->read_info.in_body.remaining == 0)
              transfer_done (xfer);
            break;
          }
        case DSK_HTTP_CLIENT_STREAM_READ_IN_BODY_EOF:
          dsk_buffer_drain (&xfer->content->buffer, &stream->incoming_data);
          break;
        case DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNKED_HEADER:
          {
            int c;
            while ((c=dsk_buffer_read_byte (&stream->incoming_data)) != -1)
              {
                if (dsk_ascii_isxdigit (c))
                  {
                    xfer->read_info.in_xfer_chunk.remaining <<= 4;
                    xfer->read_info.in_xfer_chunk.remaining += dsk_ascii_xdigit_value (c);
                  }
                else if (c == '\n')
                  {
                    if (xfer->read_info.in_xfer_chunk.remaining == 0)
                      {
                        xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_XFER_CHUNK_TRAILER;
                        xfer->read_info.in_xfer_chunk_trailer.checked = 0;
                        goto in_trailer;
                      }
                    else
                      xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNK;
                    break;
                  }
                else if (c == '\r' || c == ' ')
                  {
                    /* ignore */
                  }
                else if (c == ';')
                  {
                    xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNKED_HEADER_EXTENSION;
                    goto in_xfer_chunked_header_extension;
                  }
                else
                  {
                    client_stream_set_error (stream, xfer, "unexpected char %s in 'chunked' header", dsk_ascii_byte_name (c));
                    do_shutdown (stream);
                    return DSK_FALSE;
                  }
              }
          }
          break;
        case DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNKED_HEADER_EXTENSION:
        in_xfer_chunked_header_extension:
          {
            int nl_index;
            nl_index = dsk_buffer_index_of (&stream->incoming_data, '\n');
            if (nl_index == -1)
              {
                if (stream->incoming_data.size > MAX_XFER_CHUNKED_EXTENSION_LENGTH)
                  {
                    client_stream_set_error (stream, xfer, "in transfer-encoding-chunked header: extension too long");
                    do_shutdown (stream);
                    return DSK_FALSE;
                  }
                goto return_true;
              }
            dsk_buffer_discard (&stream->incoming_data, nl_index + 1);
            if (xfer->read_info.in_xfer_chunk.remaining == 0)
              {
                xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_XFER_CHUNK_TRAILER;
                xfer->read_info.in_xfer_chunk_trailer.checked = 0;
                goto in_trailer;

              }
            else
              xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNK;
            break;
          }

        case DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNK:
          {
            unsigned amount = xfer->read_info.in_xfer_chunk.remaining;
            if (amount > stream->incoming_data.size)
              amount = stream->incoming_data.size;
            xfer->read_info.in_xfer_chunk.remaining -= amount;
            transfer_content (xfer, amount);
          }
          dsk_memory_source_added_data (xfer->content);
          if (xfer->read_info.in_xfer_chunk.remaining == 0)
            {
              /* expect newline */
              xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_AFTER_XFER_CHUNK;
            }
          break;

        case DSK_HTTP_CLIENT_STREAM_READ_AFTER_XFER_CHUNK:
          {
            for (;;)
              {
                int c = dsk_buffer_read_byte (&stream->incoming_data);
                if (c == '\r')
                  continue;
                if (c == '\n')
                  {
                    xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNKED_HEADER;
                    xfer->read_info.in_xfer_chunk.remaining = 0;
                    break;
                  }
                if (c == -1)
                  goto return_true;
                client_stream_set_error (stream, xfer, "unexpected char %s after 'chunked' header", dsk_ascii_byte_name (c));
                do_shutdown (stream);
                return DSK_FALSE;
              }
            break;
          }
        case DSK_HTTP_CLIENT_STREAM_READ_XFER_CHUNK_TRAILER:
        in_trailer:
          /* We are looking for the end of content similar to the end
             of an http-header: two consecutive newlines. */
          if (!_dsk_http_scan_for_end_of_header (&stream->incoming_data, 
                                            &xfer->read_info.in_xfer_chunk_trailer.checked,
                                            DSK_TRUE))
            {
              goto return_true;
            }
          dsk_buffer_discard (&stream->incoming_data,
                              xfer->read_info.in_xfer_chunk_trailer.checked);

          transfer_done (xfer);
          xfer = NULL;            /* reduce chances of bugs */
          goto restart_processing;
#if 0
          /* fall-though to that case */
        case DSK_HTTP_CLIENT_STREAM_READ_XFER_CHUNK_FINAL_NEWLINE:
          {
            int c;
            while ((c = dsk_buffer_peek_byte (&stream->incoming_data)) != -1)
              {
                if (c == '\r' || c == ' ')
                  {
                    dsk_buffer_discard (&stream->incoming_data, 1);
                    continue;
                  }
                if (c == '\n')
                  {
                    dsk_buffer_discard (&stream->incoming_data, 1);
                    transfer_done (xfer);
                    xfer = NULL;            /* reduce chances of bugs */
                    goto restart_processing;
                  }
                if (!stream->strict_keepalive)
                  {
                    transfer_done (xfer);
                    xfer = NULL;            /* reduce chances of bugs */
                    goto restart_processing;
                  }
                else
                  {
                    client_stream_set_error (stream, xfer,
                                             "bad character after transfer-encoding: chunked; expected newline");
                    do_shutdown (stream);
                    return DSK_FALSE;
                  }
              }
          }
          break;
#endif
        default:
          /* INIT already handled when checking if incoming_data_transfer==NULL;
             DONE should never be encountered for incoming_data_transfer */
          dsk_assert_not_reached ();
        }
    }
return_true:
  if (got_eof)
    do_shutdown (stream);
  return DSK_TRUE;
}

static dsk_boolean handle_writable (DskOctetSink *sink,
                                    DskHttpClientStream *stream);
static void
maybe_add_write_hook (DskHttpClientStream *stream)
{
  if (stream->write_trap != NULL)
    return;
#if 0
  if (stream->outgoing_data_transfer == NULL)
    return;
#endif
  if (stream->n_pending_outgoing_requests >= stream->max_pipelined_requests)
    return;
  if (stream->sink == NULL)
    return;

  stream->write_trap = dsk_hook_trap (&stream->sink->writable_hook,
                                      (DskHookFunc) handle_writable,
                                      stream,
                                      NULL);
}

static void
write_end_transfer_encoding_chunked (DskBuffer *buffer)
{
  static const char terminator[] = "0\r\n"    /* last-chunk */
                                              /* no trailer */
                                   "\r\n";    /* terminate request */
  dsk_buffer_append (buffer, 5, terminator);
}

static dsk_boolean
handle_post_data_readable (DskOctetSource *source,
                           DskHttpClientStreamTransfer *xfer)
{
  DskBuffer buffer = DSK_BUFFER_STATIC_INIT;
  DskBuffer compressed_buffer = DSK_BUFFER_STATIC_INIT;
  DskBuffer *buf = &buffer;
  DskError *error = NULL;
  DskHttpClientStream *stream = xfer->owner;
  if (xfer->post_data_encoder != NULL)
    buf = &compressed_buffer;
  switch (dsk_octet_source_read_buffer (source, &buffer, &error))
    {
    case DSK_IO_RESULT_SUCCESS:
      if (buffer.size == 0)
        return DSK_TRUE;
      if (xfer->post_data_encoder != NULL)
        {
          DskError *error = NULL;
          if (!dsk_octet_filter_process_buffer (xfer->post_data_encoder,
                                                &compressed_buffer,
                                                buffer.size, &buffer,
                                                DSK_TRUE, &error))
            dsk_die ("error compressing: %s", error->message);
          if (compressed_buffer.size == 0)
            return DSK_TRUE;
        }
      if (xfer->request->transfer_encoding_chunked)
        {
          char chunked_header[MAX_CHUNK_HEADER_SIZE];
          unsigned len = get_chunked_header (buf->size, chunked_header);
          dsk_buffer_append (&stream->outgoing_data, len, chunked_header);
        }
      dsk_buffer_drain (&stream->outgoing_data, buf);
      if (xfer->request->transfer_encoding_chunked)
        dsk_buffer_append (&stream->outgoing_data, 2, "\r\n");

#if 0 /* DEBUG: Dump buffer */
      dsk_print_push (NULL);
      dsk_print_set_quoted_buffer (NULL, "buffer", &stream->outgoing_data,
                                DSK_PRINT_STRING_C_QUOTED);
      dsk_print (NULL, "handle_post_data_readable: now outgoing buffer is $buffer");
      dsk_print_pop (NULL);
#endif

      maybe_add_write_hook (stream);
      if (stream->outgoing_data.size > stream->max_outgoing_data)
        {
          xfer->write_info.in_content.post_data_trap = NULL;
          return DSK_FALSE;
        }
      else
        return DSK_TRUE;
    case DSK_IO_RESULT_AGAIN:
      return DSK_TRUE;
    case DSK_IO_RESULT_EOF:
      if (xfer->request->transfer_encoding_chunked)
        write_end_transfer_encoding_chunked (&stream->outgoing_data);

      stream->outgoing_data_transfer = xfer->next;

      maybe_add_write_hook (stream);
      xfer->write_info.in_content.post_data_trap = NULL;

      if (xfer->post_data)
        {
          dsk_octet_source_shutdown (xfer->post_data);
          dsk_object_unref (xfer->post_data);
          xfer->post_data = NULL;
        }

      return DSK_FALSE;
    case DSK_IO_RESULT_ERROR:
      client_stream_set_error (stream, xfer,
                               "error reading post-data: %s",
                               error->message);
      dsk_error_unref (error);
      xfer->write_info.in_content.post_data_trap = NULL;
      do_shutdown (stream);
      return DSK_FALSE;
    }
  dsk_return_val_if_reached ("bad io-result writing post-data", DSK_FALSE);
}

static dsk_boolean
handle_writable (DskOctetSink *sink,
                 DskHttpClientStream *stream)
{
  DskError *error = NULL;
  dsk_boolean blocked = DSK_FALSE;

  while (stream->outgoing_data.size < stream->max_outgoing_data
      && stream->outgoing_data_transfer != NULL
      && !blocked)
    {
      DskHttpClientStreamTransfer *xfer = stream->outgoing_data_transfer;
      switch (xfer->write_state)
        {
        case DSK_HTTP_CLIENT_STREAM_WRITE_INIT:
          /* serialize header and write it to buffer */
          dsk_http_request_print_buffer (xfer->request, &stream->outgoing_data);
          dsk_buffer_append (&stream->outgoing_data, 2, "\r\n");
          if (xfer->post_data)
            {
              xfer->write_state = DSK_HTTP_CLIENT_STREAM_WRITE_CONTENT;
              xfer->write_info.in_content.bytes = 0;
              xfer->write_info.in_content.post_data_trap = NULL;
            }
          else
            xfer->write_state = DSK_HTTP_CLIENT_STREAM_WRITE_DONE;
          break;
        case DSK_HTTP_CLIENT_STREAM_WRITE_CONTENT:
          {
            DskBuffer *buf;
            DskBuffer b = DSK_BUFFER_STATIC_INIT;
            unsigned old_buf_size;
            /* continue writing body */
            if (xfer->request->transfer_encoding_chunked)
              buf = &b;
            else
              buf = &stream->outgoing_data;
            old_buf_size = buf->size;

            switch (dsk_octet_source_read_buffer (xfer->post_data,
                                                  buf,
                                                  &error))
              {
              case DSK_IO_RESULT_SUCCESS:
                {
                  unsigned n_post_data = buf->size - old_buf_size;
                  xfer->write_info.in_content.bytes += n_post_data;
                  if (xfer->request->content_length >= 0
                   && xfer->write_info.in_content.bytes > (uint64_t) xfer->request->content_length)
                    {
                      /* post-data longer than expected */
                      client_stream_set_error (stream, xfer,
                                               "got %llu bytes for a POST whose Content-Length was %llu",
                                               xfer->write_info.in_content.bytes,
                                               (uint64_t) xfer->request->content_length);
                      do_shutdown (stream);
                      return DSK_FALSE;
                    }
                }
                if (xfer->request->transfer_encoding_chunked
                 && buf->size != 0)
                  {
                    char chunked_header[MAX_CHUNK_HEADER_SIZE];
                    unsigned at = get_chunked_header (buf->size, chunked_header);
                    dsk_buffer_append (&stream->outgoing_data, at, chunked_header);
                    dsk_buffer_drain (&stream->outgoing_data, buf);
                    dsk_buffer_append (&stream->outgoing_data, 2, "\r\n");
                  }
                break;
              case DSK_IO_RESULT_AGAIN:
                /* trap post-data readable */
                blocked = DSK_TRUE;
                if (xfer->write_info.in_content.post_data_trap == NULL)
                  xfer->write_info.in_content.post_data_trap
                    = dsk_hook_trap (&xfer->post_data->readable_hook,
                                     (DskHookFunc) handle_post_data_readable,
                                     xfer, NULL);
                break;
              case DSK_IO_RESULT_EOF:
                if (xfer->request->content_length >= 0
                 && xfer->write_info.in_content.bytes < (uint64_t) xfer->request->content_length)
                  {
                    client_stream_set_error (stream, xfer,
                                             "got too few (%llu) bytes for a POST whose Content-Length was %llu",
                                             xfer->write_info.in_content.bytes,
                                             (uint64_t) xfer->request->content_length);
                    do_shutdown (stream);
                    return DSK_FALSE;
                  }
                if (xfer->request->transfer_encoding_chunked)
                  write_end_transfer_encoding_chunked (&stream->outgoing_data);
                if (xfer->write_info.in_content.post_data_trap != NULL)
                  {
                    dsk_hook_trap_destroy (xfer->write_info.in_content.post_data_trap);
                    xfer->write_info.in_content.post_data_trap = NULL;
                  }
                if (xfer->post_data)
                  {
                    dsk_octet_source_shutdown (xfer->post_data);
                    dsk_object_unref (xfer->post_data);
                    xfer->post_data = NULL;
                  }
                xfer->write_state = DSK_HTTP_CLIENT_STREAM_WRITE_DONE;
                stream->outgoing_data_transfer = xfer->next;
                break;
              case DSK_IO_RESULT_ERROR:
                client_stream_set_error (stream, xfer,
                                         "error reading post-data: %s",
                                         error->message);
                dsk_error_unref (error);
                do_shutdown (stream);
                return DSK_FALSE;
              }
          }
          break;
        default:
          dsk_assert_not_reached ();
        }
      if (xfer->write_state == DSK_HTTP_CLIENT_STREAM_WRITE_DONE)
        {
          stream->outgoing_data_transfer = xfer->next;
          --stream->n_pending_outgoing_requests;
        }
    }

  if (stream->outgoing_data_transfer == NULL)
    blocked = DSK_TRUE;
  switch (dsk_octet_sink_write_buffer (sink, &stream->outgoing_data, &error))
    {
    case DSK_IO_RESULT_SUCCESS:
      break;
    case DSK_IO_RESULT_AGAIN:
      break;
    case DSK_IO_RESULT_EOF:
      /* treat as error */
      error = dsk_error_new ("writing to http-client got EOF");
      /* fall-through */
    case DSK_IO_RESULT_ERROR:
      client_stream_set_error (stream, stream->outgoing_data_transfer,
                               "error writing to http-client sink: %s",
                               error->message);
      dsk_error_unref (error);
      break;
    }

  if (blocked && stream->outgoing_data.size == 0)
    {
      stream->write_trap = NULL;
      return DSK_FALSE;
    }

  return DSK_TRUE;
}

static void dsk_http_client_stream_init (DskHttpClientStream *stream)
{
  dsk_hook_init (&stream->error_hook, stream);
  DSK_UNUSED (stream);
}
static void dsk_http_client_stream_finalize (DskHttpClientStream *stream)
{
  do_shutdown (stream);
  if (stream->latest_error)
    dsk_error_unref (stream->latest_error);
  dsk_hook_clear (&stream->error_hook);
  dsk_buffer_clear (&stream->incoming_data);
  dsk_buffer_clear (&stream->outgoing_data);
}

DSK_OBJECT_CLASS_DEFINE_CACHE_DATA (DskHttpClientStream);
const DskHttpClientStreamClass dsk_http_client_stream_class =
{
  DSK_OBJECT_CLASS_DEFINE (DskHttpClientStream,
                           &dsk_object_class,
                           dsk_http_client_stream_init,
                           dsk_http_client_stream_finalize)
};

DskHttpClientStream *
dsk_http_client_stream_new     (DskOctetSink        *sink,
                                DskOctetSource      *source,
                                const DskHttpClientStreamOptions *options)
{
  DskHttpClientStream *stream = dsk_object_new (&dsk_http_client_stream_class);
  stream->sink = dsk_object_ref (sink);
  stream->source = dsk_object_ref (source);
  stream->read_trap = dsk_hook_trap (&source->readable_hook,
                                     (DskHookFunc) handle_transport_source_readable,
                                     stream,
                                     NULL);
  stream->max_header_size = options->max_header_size;
  stream->max_pipelined_requests = options->max_pipelined_requests;
  stream->max_outgoing_data = options->max_outgoing_data;
  stream->strict_keepalive = options->strict_keepalive ? 1 : 0;
  stream->print_warnings = options->print_warnings ? 1 : 0;
  return stream;
}

DskHttpClientStreamTransfer *
dsk_http_client_stream_request (DskHttpClientStream      *stream,
                                DskHttpClientStreamRequestOptions *options,
                                DskError          **error)
{
  DskHttpClientStreamTransfer *xfer;
  DskHttpRequest *request;
  DskBuffer compressed_data = DSK_BUFFER_STATIC_INIT;
  if (options->post_data_len >= 0
   && (options->post_data_len == 0 || options->post_data_slab != NULL)
   && options->gzip_compress_post_data)
    {
      /* compress post-data immediately */
      DskOctetFilter *filter = dsk_zlib_compressor_new (DSK_ZLIB_GZIP, options->gzip_compression_level);
      if (!dsk_octet_filter_process (filter, &compressed_data,
                                     options->post_data_len, options->post_data_slab, error)
       || !dsk_octet_filter_finish (filter, &compressed_data, error))
        {
          dsk_object_unref (filter);
          return NULL;
        }
    }
  if (options->request != NULL)
    {
      /* --- verify existing header is valid --- */
      if (options->post_data_len >= 0)
        {
          if (options->post_data_is_gzipped || !options->gzip_compress_post_data)
            {
              if (options->post_data_is_gzipped)
                dsk_assert (options->request->content_encoding_gzip);
              dsk_assert (options->request->transfer_encoding_chunked
                       || options->request->content_length == options->post_data_len);
            }
          else
            {
              dsk_assert (options->request->transfer_encoding_chunked);
            }
        }
      else if (options->post_data)
        {
          dsk_assert (options->request->content_length != -1LL
                  ||  options->request->transfer_encoding_chunked);
        }
      request = dsk_object_ref (options->request);
    }
  else
    {
      /* --- construct request header --- */
      DskHttpRequestOptions ropts = *options->request_options;

      if (options->gzip_compress_post_data || options->post_data_is_gzipped)
        ropts.content_encoding_gzip = 1;
      if (options->post_data_len >= 0)
        {
          if (options->gzip_compress_post_data)
            ropts.content_length = compressed_data.size;
          else
            ropts.content_length = options->post_data_len;
        }
      else
        {
          ropts.transfer_encoding_chunked = 1;
        }
      request = dsk_http_request_new (&ropts, error);
      if (request == NULL)
        return NULL;
    }
  DskOctetSource *post_data;
  if (options->post_data != NULL)
    {
      if (options->gzip_compress_post_data)
        {
          /* create filtered source */
          DskOctetFilter *filter = dsk_zlib_compressor_new (DSK_ZLIB_GZIP, options->gzip_compression_level);
          post_data = dsk_octet_filter_source (options->post_data, filter);
          dsk_object_unref (filter);
        }
      else
        {
          post_data = dsk_object_ref (options->post_data);
        }
    }
  else if (options->post_data_len >= 0)
    {
      DskMemorySource *source = dsk_memory_source_new ();
      if (options->gzip_compress_post_data)
        /* use compressed_data */
        dsk_buffer_drain (&source->buffer, &compressed_data);
      else
        /* create stream (?) */
        dsk_buffer_append (&source->buffer, options->post_data_len, options->post_data_slab);
      dsk_memory_source_added_data (source);
      post_data = DSK_OCTET_SOURCE (source);
    }
  else
    post_data = NULL;

  xfer = dsk_malloc (sizeof (DskHttpClientStreamTransfer));
  GSK_QUEUE_ENQUEUE (GET_STREAM_XFER_QUEUE (stream), xfer);
  if (stream->outgoing_data_transfer == NULL)
    stream->outgoing_data_transfer = xfer;
  stream->n_pending_outgoing_requests++;
  xfer->owner = stream;
  xfer->request = request;
  xfer->post_data = post_data;
  xfer->funcs = options->funcs;
  xfer->user_data = options->user_data;
  xfer->response = NULL;
  xfer->content = NULL;
  xfer->content_decoder = NULL;
  xfer->post_data_encoder = NULL;
  xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_NEED_HEADER;
  xfer->read_info.need_header.checked = 0;
  xfer->write_state = DSK_HTTP_CLIENT_STREAM_WRITE_INIT;
  xfer->failed = 0;
  xfer->uncompress_content = options->uncompress_content;
  maybe_add_write_hook (stream);
  return xfer;
}
