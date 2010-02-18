
static inline void
transfer_done (DskHttpClientStreamTransfer *xfer)
{
  if (xfer->funcs->handle_content_complete != NULL)
    xfer->funcs->handle_content_complete (xfer);
  if (xfer->funcs->destroy != NULL)
    xfer->funcs->destroy (xfer);
  dsk_free (xfer);
}
static dsk_boolean
handle_transport_source_readable (DskOctetSource *source,
                                  void           *data)
{
  DskHttpClientStream *stream = data;
  int rv;
  DskError *error = NULL;
  rv = dsk_octet_source_read_buffer (source, &stream->incoming_data, &error);
  if (rv < 0)
    {
      client_stream_set_error (stream, "error reading from underlying transport: %s",
                               error->message);
      dsk_error_unref (error);
      return DSK_TRUE;                  /* ??? */
    }
  if (rv == 0)
    return DSK_TRUE;
  while (stream->incoming_data.size > 0)
    {
      DskHttpClientStreamTransfer *xfer = stream->first_transfer;
      if (xfer == NULL
       || xfer->read_state == DSK_HTTP_CLIENT_STREAM_READ_INIT)
        {
          client_stream_set_error (stream, "got data when none expected");
          do_shutdown (stream);             /* ??? */
          return DSK_TRUE;                  /* ??? */
        }
      switch (xfer->read_state)
        {
          /* The key pattern we are looking for is:
                \n \r? \n
           */
        case DSK_HTTP_CLIENT_STREAM_READ_NEED_HEADER:
          {
            DskBufferFragment *frag;
            unsigned frag_offset;
            unsigned start = xfer->read_info.need_header.start_check;
            if (start == stream->incoming_data.size)
              break;
            if (!dsk_buffer_peek_fragment (&stream->incoming_data, start,
                                           &frag, &frag_offset))
              dsk_assert_not_reached ();
            /* state 0:  non-\n
               state 1:  \n
               state 2:  \n \r
             */
            unsigned state = 0;
            while (frag != NULL)
              {
                char *at = frag->buf + frag_offset;
                char *end = frag->buf + frag->start + frag->len;
                while (at < end)
                  {
                    if (*at == '\n')
                      {
                        if (state == 0)
                          state = 1;
                        else
                          {
                            at++;
                            start++;
                            goto got_header;
                          }
                      }
                    else if (*at == '\r')
                      {
                        if (state == 1)
                          state = 2;
                        else
                          state = 0;
                      }
                    else
                      state = 0;
                    at++;
                    start++;
                  }
              }
            if (state == 1)
              start--;
            else if (state == 2)
              start -= 2;
            xfer->read_info.need_header.start_check = start;
            return DSK_TRUE;

got_header:
            /* parse header */
            response = dsk_http_response_parse_buffer (&stream->incoming_data, start, &error);
            dsk_buffer_discard (&stream->incoming_data, start);
            if (response == NULL)
              {
                ...
              }
            xfer->response = response;
            if (has_body (xfer->request, xfer->response))
              {
                /* setup (empty) response stream */
                ...

                if (xfer->response->transfer_encoding_chunked)
                  {
                    xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNKED_HEADER;
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

                    stream->incoming_data_transfer = xfer->next;
                  }
                else
                  {
                    if (!xfer->response->connection_close)
                      {
                        ...
                      }
                    xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_IN_BODY_EOF;
                  }
              }
            else
              {
                xfer->read_state = DSK_HTTP_CLIENT_STREAM_READ_DONE;
                stream->incoming_data_transfer = xfer->next;
              }
            /* notify */
            if (xfer->funcs->handle_response != NULL)
              xfer->funcs->handle_response (xfer);
            if (xfer->read_state == DSK_HTTP_CLIENT_STREAM_READ_DONE)
              transfer_done (xfer);             /* frees transfer */
          }
          break;
        case DSK_HTTP_CLIENT_STREAM_READ_IN_BODY:
          ...
          break;
        case DSK_HTTP_CLIENT_STREAM_READ_IN_BODY_EOF:
          ...
          break;
        case DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNKED_HEADER:
          ...
          break;
        case DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNK:
          ...
          break;
        default:
          /* INIT already handled when checking if incoming_data_transfer==NULL;
             DONE should never be encountered for incoming_data_transfer */
          dsk_assert_not_reached ();
        }
    }
  return DSK_TRUE;
}

static void
handle_transport_source_read_destroy (void *data)
{
  DskHttpClientStream *stream = data;
  ...
}

DskHttpClientStream *
dsk_http_client_stream_new     (DskOctetSink        *sink,
                                DskOctetSource      *source)
{
  DskHttpClientStream *stream = dsk_object_new (&dsk_http_client_stream_class);
  stream->sink = dsk_object_ref (sink);
  stream->source = dsk_object_ref (source);
  stream->read_trap = dsk_hook_trap_readable (&source->readable_hook,
                                              handle_transport_source_readable,
                                              stream,
                                              handle_transport_source_read_destroy);
  return stream;
}

DskHttpClientStreamTransfer *
dsk_http_client_stream_request (DskHttpClientStream      *stream,
                                DskHttpRequest           *request,
				DskOctetSource           *post_data,
				DskHttpClientStreamFuncs *funcs,
				void                     *user_data)
{
  ...
}
