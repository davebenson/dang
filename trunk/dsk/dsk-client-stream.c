#include "dsk.h"

/* Begin connecting to the given resolved address. */
static void begin_connecting           (DskClientStream *stream);

/* Create the barebones client-stream objects. */
static DskClientStream *create_raw_client_stream (void)
{
  DskClientStream *rv = dsk_object_new (&dsk_client_stream_class);
  rv->sink = dsk_object_new (&dsk_client_stream_sink_class);
  rv->source = dsk_object_new (&dsk_client_stream_source_class);
  rv->sink->owner = rv->source->owner = rv;
  rv->reconnect_time_ms = -1;
  rv->idle_disconnect_time_ms = -1;
  return rv;
}

DskClientStream *
dsk_client_stream_new       (const char *name,
                             unsigned    port,
                             DskError  **error)
{
  DskClientStream *rv = create_raw_client_stream ();
  if (dsk_hostname_looks_numeric (name))
    rv->is_numeric_name = 1;
  rv->name = dsk_strdup (name);
  rv->port = port;
  begin_connecting (rv);
  return rv;
}

DskClientStream *
dsk_client_stream_new_addr  (DskDnsAddress *addr,
                             unsigned       port,
                             DskError     **error)
{
  DskClientStream *rv = create_raw_client_stream ();
  rv->is_numeric_name = 1;
  rv->name = dsk_dns_address_to_string (addr);
  rv->port = port;
  begin_connecting (rv);
  return rv;
}

DskClientStream *dsk_client_stream_new_local (const char *path)
{
  DskClientStream *rv = create_raw_client_stream ();
  rv->is_local_socket = 1;
  rv->name = dsk_strdup (path);
  begin_connecting (rv);
  return rv;
}

/* use -1 to disable these timeouts */
void
dsk_client_stream_set_reconnect_time (DskClientStream *client,
                                      int              millis)
{
  ...
}

void
dsk_client_stream_set_max_idle_time  (DskClientStream *client,
                                      int              millis)
{
  ...
}

/* --- connecting & dns lookups --- */
static void
begin_connecting_sockaddr (DskClientStream *stream,
                           unsigned          addr_len,
                           struct sockaddr_t *addr)
{
  int fd;
retry_sys_socket:
  fd = socket (addr->sa_family, SOCK_STREAM, 0);
  if (fd < 0)
    {
      if (errno == EINTR)
        goto retry_sys_socket;

      /* set error */
      ...

      goto error;
    }

  /* set non-blocking */
  ...

  /* call connect() */
retry_sys_connect:
  if (connect (fd, addr, addr_len) < 0)
    {
      int e = errno;
      if (e == EINTR)
        goto retry_sys_connect;
      
      if (e == EAGAIN)
        {
          stream->is_connecting = 1;
          stream->fd = fd;
          dsk_dispatch_watch_fd (dsk_dispatch_default (), fd,
                                 DSK_EVENT_WRITABLE|DSK_EVENT_READABLE,
                                 handle_fd_connecting, stream);
          return;
        }

      /* set error */
      ...

      goto error;
    }

  stream->is_connected = 1;
  stream->fd = fd;
  events = 0;
  if (dsk_hook_is_trapped (stream->readable_hook))
    events |= DSK_EVENT_READABLE;
  if (dsk_hook_is_trapped (stream->writable_hook))
    events |= DSK_EVENT_WRITABLE;
  dsk_dispatch_watch_fd (dsk_dispatch_default (), fd, events,
                         handle_fd_events, stream);
  return;

error:
  /* notify error hook */
  ...

  maybe_set_autoreconnect_timer (stream);
  return;
}
#if 0
static void
begin_connecting_dns_entry (DskClientStream *stream,
                            DskDnsAddress   *address,
                            unsigned         port)
{
  unsigned addr_len;
  struct sockaddr_storage addr;
  stream->is_connecting = 1;
  dsk_dns_address_to_sockaddr (address, port, &addr_len, &addr);
  begin_connecting_sockaddr (stream, addr_len, (struct sockaddr *) &addr);
}
#endif

static void
begin_connecting (DskClientStream *stream)
{
  if (stream->is_local_socket)
    {
      ...
    }
  else if (stream->is_numeric_name)
    {
      ...
    }
  else
    {
      /* begin dns lookup */
      ...
    }
}

DskClientStreamClass dsk_client_stream_class =
{
  DSK_OBJECT_CLASS_DEFINE(DskClientStream, &dsk_object_class,
                          NULL, dsk_client_stream_finalize)
};


/* === Implementation of octet-source class === */
static int
dsk_client_stream_source_read (DskOctetSource *source,
                               unsigned        max_len,
                               void           *data_out,
                               DskError      **error)
{
  ...
}

static int
dsk_client_stream_source_read_buffer  (DskOctetSource *source,
                                       DskBuffer      *read_buffer,
                                       DskError      **error)
{
  ...
}

static void
dsk_client_stream_source_shutdown (DskOctetSource *source)
{
  ...
}


DskClientStreamSinkClass dsk_client_stream_source_class =
{
  { DSK_OBJECT_CLASS_DEFINE (DskClientStreamSource, &dsk_octet_source_class,
                             NULL, NULL),
    dsk_client_stream_source_read,
    dsk_client_stream_source_read_buffer,
    dsk_client_stream_source_shutdown,
  }
};

/* === Implementation of octet-sink class === */
static int
dsk_client_stream_sink_write  (DskOctetSink   *sink,
                               unsigned        max_len,
                               const void     *data_out,
                               DskError      **error)
{
  ...
}

static int
dsk_client_stream_sink_write_buffer  (DskOctetSink   *sink,
                                      DskBuffer      *write_buffer,
                                      DskError      **error)
{
  ...
}

static void
dsk_client_stream_sink_shutdown   (DskOctetSink   *sink)
{
  ...
}

DskClientStreamSinkClass dsk_client_stream_sink_class =
{
  { DSK_OBJECT_CLASS_DEFINE (DskClientStreamSink, &dsk_octet_sink_class,
                             NULL, NULL),
    dsk_client_stream_sink_write,
    dsk_client_stream_sink_write_buffer,
    dsk_client_stream_sink_shutdown,
  }
};

