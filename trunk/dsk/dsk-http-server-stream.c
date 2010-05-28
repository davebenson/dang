#include "dsk.h"
#include "dsk-http-internals.h"

static void
server_set_error (DskHttpServerStream *server,
                  const char          *format,
                  ...)
{
  DskHttpServerStreamTransfer *xfer;
  while ((xfer = server->first_transfer) != NULL)
    {
      /* mark all transfers as defunct */
      ...

    }


}

static void
do_shutdown (DskHttpServerStream *server)
{
  while (server->first_transfer)
  ...
}

static dsk_boolean
handle_source_readable (DskOctetSource *source,
                        DskHttpServerStream *ss)
{
  switch (dsk_octet_source_read_buffer (source, &ss->incoming_data))
    {
    case DSK_IO_RESULT_SUCCESS:
    case DSK_IO_RESULT_AGAIN:
      return DSK_TRUE;
    case DSK_IO_RESULT_EOF:
      if (ss->first_transfer)
        server_set_error (...);
      do_shutdown (ss);
      return DSK_FALSE;
    case DSK_IO_RESULT_ERROR:
      server_set_error (...);
      do_shutdown (ss);
      return DSK_FALSE;
    }
  if (ss->incoming_data.size == 0)
    return DSK_TRUE;
  if (ss->read_transfer == NULL)
    {
      /* new transfer */
      DskHttpServerStreamTransfer *xfer = dsk_malloc (sizeof (DskHttpServerStreamTransfer));
      xfer->owner = ss;
      xfer->request = NULL;
      xfer->response = NULL;
      xfer->content = NULL;
      xfer->read_state = DSK_HTTP_SERVER_STREAM_READ_NEED_HEADER;
      xfer->read_info.need_header.checked = 0;
      xfer->returned = xfer->responded = 0;

      GSK_QUEUE_APPEND (GET_XFER_QUEUE (ss), xfer);
      ss->read_transfer = xfer;
    }
restart_processing:
  switch (ss->read_transfer->read_state)
    {
    case DSK_HTTP_SERVER_STREAM_READ_NEED_HEADER:
      {
        unsigned start = xfer->read_info.need_header.checked;
        if (start == stream->incoming_data.size)
          break;

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
        else
          {
            /* parse header */
            ...

            if (!server->wait_for_post_complete)
              {
                if (server->next_response_to_return == NULL)
                  server->next_response_to_return = xfer;

                /* notify that we have a header */
                dsk_hook_set_idle_notify (&ss->request_available, DSK_TRUE);
              }

            /* transition states */
            ...
          }
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
            ...
          }
        goto return_true;
      }
    case DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNKED_HEADER:
      {
        while ((c=dsk_buffer_read_byte (&ss->incoming_data)) != -1)
          {
            if (dsk_ascii_isxdigit (c))
              {
                ss->read_state.in_xfer_chunk.remaining <<= 4;
                ss->read_state.in_xfer_chunk.remaining += dsk_ascii_xdigit_value (c);
              }
            else if (c == '\n')
              {
                /* switch state */
                ...
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
                do_shutdown (ss);
                return DSK_FALSE;
              }
          }
        goto return_true;
      }
    case DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNKED_HEADER_EXTENSION:
    in_xfer_chunked_header_extension:
      while ((c=dsk_buffer_read_byte (&ss->incoming_data)) != -1)
        {
          if (c == '\n')
            {
              /* switch state */
              ...
            }
          else
            {
              /* do nothing */
            }
        }
      goto return_true;
    case DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNK:
      ...
    case DSK_HTTP_SERVER_STREAM_READ_AFTER_XFER_CHUNK:
      ...
    case DSK_HTTP_SERVER_STREAM_READ_AFTER_XFER_CHUNKED:
      ...
    case DSK_HTTP_SERVER_STREAM_READ_XFER_CHUNK_TRAILER:
      ...
    case DSK_HTTP_CLIENT_STREAM_READ_XFER_CHUNK_FINAL_NEWLINE:
      ...
    case DSK_HTTP_SERVER_STREAM_READ_DONE:
      dsk_assert_not_reached ();
    }
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

DskHttpServerStreamTransfer *
dsk_http_server_stream_get_request (DskHttpServerStream *stream)
{
  ...
  
  if (stream->next_response_to_return == NULL)
    dsk_hook_set_idle_notify (&ss->request_available, DSK_FALSE);
}

#if 0           /* for detecting if POST data is done? */
void
dsk_http_server_stream_set_funcs (DskHttpServerStreamTransfer      *transfer,
                                  DskHttpServerStreamResponseFuncs *funcs,
                                  void                             *func_data);
{
  ...
}
#endif


void
dsk_http_server_stream_respond (DskHttpServerStreamTransfer *transfer,
                                DskHttpServerStreamResponseOptions *options)
{
  ...
}
