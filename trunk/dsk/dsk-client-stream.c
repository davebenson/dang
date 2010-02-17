#include "dsk.h"

/* Begin connecting to the given resolved address. */
static void begin_connecting           (DskClientStream *stream);

/* Create the barebones client-stream objects. */
static DskClientStream *
create_raw_client_stream (void)
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
  if (hostname_looks_numeric (name))
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

DskClientStream *
dsk_client_stream_new_local (const char *path)
{
  DskClientStream *rv = create_raw_client_stream ();
  rv->is_local_socket = 1;
  rv->name = dsk_strdup (path);
  begin_connecting (rv);
  return rv;
}

/* numeric hostnames */
static dsk_boolean
hostname_looks_numeric (const char *str)
{
  const char *at;

  at = str;
  SKIP_WHITESPACE (at);
  for (i = 0; i < 3; i++)
    {
      if (!ascii_is_digit (*at))
        goto is_not_ipv4;
      while (ascii_is_digit (*at))
        at++;
      SKIP_WHITESPACE (at);
      if (*at != '.')
        goto is_not_ipv4;
      at++;
      SKIP_WHITESPACE (at);
    }
  if (!ascii_is_digit (*at))
    goto is_not_ipv4;
  while (ascii_is_digit (*at))
    at++;
  SKIP_WHITESPACE (at);
  if (*at != 0)
    goto is_not_ipv4;
  return DSK_TRUE;              /* ipv4 */

is_not_ipv4:
  for (i = 0; i < 4; i++)
    {
      for (j = 0; j < 4; j++)
        {
          if (!ascii_is_xdigit (*at))
            goto is_not_ipv6;
          at++;
        }
      if (*at != ':')
        goto is_not_ipv6;
      at++;
    }
  return DSK_TRUE;

is_not_ipv6:
  return DSK_FALSE;
}

static dsk_boolean
parse_numeric_ip (const char *str,
                  DskDnsAddress *out)
{
  ...
}

/* use -1 to disable these timeouts */
void
dsk_client_stream_set_reconnect_time (DskClientStream *client,
                                      int              millis)
{
  /* short-circuit no-op cases */
  if (millis < 0)
    {
      if (client->reconnect_time_ms == -1)
        return;
      millis = -1;
    }
  else if (client->reconnect_time_ms == millis)
    return;

  /* if we have a valid file-descriptor or we are resolving the name,
     there then the reconnect_time_ms is not currently relevant:
     set it and go */
  if (client->fd != -1 || client->is_resolving_name)
    {
      client->reconnect_time_ms = millis;
      return;
    }

  if (millis == -1)
    {
      /* handle timer removal */
      if (client->reconnect_timer)
        {
          dsk_dispatch_remove_timer (client->reconnect_timer);
          client->reconnect_timer = NULL;
        }
      else
        dsk_soft_should_not_happen ("no reconnect timer?");
    }
  else
    {
      if (client->reconnect_time_ms >= 0)
        dsk_dispatch_remove_timer (client->reconnect_timer);
      else
        dsk_assert (client->reconnect_timer == NULL);
      client->reconnect_timer
        = dsk_dispatch_add_timer_millis (dsk_dispatch_default (),
                                         millis,
                                         handle_reconnect_timer_expired,
                                         client);
    }
}

void
dsk_client_stream_set_max_idle_time  (DskClientStream *client,
                                      int              millis)
{
  ...
}

/* --- connecting & dns lookups --- */
static void
stream_set_last_error (DskClientStream *stream,
                       const char      *format,
                       ...)
{
  va_list args;
  va_start (args, format);
  if (stream->last_error)
    dsk_error_unref (stream->last_error);
  stream->last_error = dsk_error_new_valist (stream, format, args);
  va_end (args);
}

int
dsk_errno_from_fd (int fd)
{
  socklen_t size_int = sizeof (int);
  int value = EINVAL;
  if (getsockopt (fd, SOL_SOCKET, SO_ERROR, &value, &size_int) < 0)
    {
      /* Note: this behavior is vaguely hypothetically broken,
       *       in terms of ignoring getsockopt's error;
       *       however, this shouldn't happen, and EINVAL is ok if it does.
       *       Furthermore some broken OS's return an error code when
       *       fetching SO_ERROR!
       */
      return value;
    }

  return value;
}

static void
handle_fd_connected (DskClientStream *stream)
{
  int events = 0;
  if (dsk_hook_is_trapped (stream->readable_hook))
    events |= DSK_EVENT_READABLE;
  if (dsk_hook_is_trapped (stream->writable_hook))
    events |= DSK_EVENT_WRITABLE;
  dsk_dispatch_watch_fd (dsk_dispatch_default (), fd, events,
                         handle_fd_events, stream);
}

static void
handle_fd_connecting (DskFileDescriptor   fd,
                      unsigned            events,
                      void               *callback_data)
{
  int err = dsk_errno_from_fd (fd);
  if (err == 0)
    {
      stream->is_connecting = DSK_FALSE;
      stream->is_connected = DSK_TRUE;
      handle_fd_connected (stream);             /* sets the watch on the fd */
      return;
    }

  if (err != EINTR && err != EAGAIN)
    {
      if (stream->last_error)
        dsk_error_unref (stream->last_error);
      stream->last_error = dsk_error_new ("error finishing connection to %s: %s",
                                          stream->name, strerror (err));
      dsk_dispatch_close_fd (dsk_dispatch_default (), stream->fd);
      stream->fd = -1;
      stream->is_connecting = DSK_FALSE;
      maybe_set_autoreconnect_timer (stream);
      return;
    }

  /* wait for another notification */
  return;
}

static void
begin_connecting_sockaddr (DskClientStream *stream,
                           unsigned          addr_len,
                           struct sockaddr_t *addr)
{
  int fd;
  dsk_assert (stream->fd == -1);
retry_sys_socket:
  fd = socket (addr->sa_family, SOCK_STREAM, 0);
  if (fd < 0)
    {
      int e = errno;
      if (e == EINTR)
        goto retry_sys_socket;
      if (dsk_fd_creation_failed (e))
        goto retry_sys_socket;

      /* set error */
      stream_set_last_error (stream,
                             "error invoking socket(2) system-call: %s",
                             strerror (errno));

      goto handle_error;
    }

  /* set non-blocking */
  dsk_fd_set_nonblocking (fd);
  dsk_fd_set_close_on_exec (fd);

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
      close (fd);
      stream_set_last_error (stream,
                             "error connecting to %s: %s",
                             stream->name, strerror (e));
      goto handle_error;
    }

  stream->is_connected = 1;
  stream->fd = fd;
  handle_fd_connected (stream);
  return;

error:
  dsk_hook_notify (stream->error_hook);
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
dns_address_to_sock_addr (DskDnsAddress *address,
                          unsigned port,
                          struct sockaddr_storage *out,
                          unsigned *out_len);
{
  /* necessary? probably not for ipv4 */
  memset (out, 0, sizeof (struct sockaddr_storage));

  switch (address->type)
    {
    case DSK_DNS_ADDRESS_IPV4:
      {
        struct sockaddr_in *a = (struct sockaddr_in *) out;
        a->sin_family = PF_INET;
        a->sin_port = htons (port);
        memcpy (&a->sin_addr, address->info.address, 4);
        break;
      }
    case DSK_DNS_ADDRESS_IPV6:
      {
        struct sockaddr_in6 *a = (struct sockaddr_in6 *) out;
        a->sin6_family = PF_INET6;
        a->sin6_port = htons (port);
        memcpy (&a->sin6_addr, address->info.address, 16);
        break;
      }
    default:
      dsk_assert_not_reached ();
    }
}

static void
handle_dns_done (DskDnsLookupResult *result,
                 void               *callback_data)
{
  DskClientStream *stream = callback_data;

  result->is_resolving_name = 0;
  switch (result->type)
    {
    case DSK_DNS_LOOKUP_RESULT_FOUND:
      {
        struct sockaddr_storage addr;
        unsigned addr_len;
        dns_address_to_sock_addr (result->addr, stream->port, &addr, &addr_len);
        begin_connecting_sockaddr (stream, addr_len, (struct sockaddr *) &addr);
      }
      break;
    case DSK_DNS_LOOKUP_RESULT_NOT_FOUND:
      stream_set_last_error (stream,
                             "dns entry for %s not found",
                             stream->name);
      maybe_set_autoreconnect_timer (stream);
      break;
    case DSK_DNS_LOOKUP_RESULT_TIMEOUT:
      stream_set_last_error (stream,
                             "dns lookup for %s timed out",
                             stream->name);
      maybe_set_autoreconnect_timer (stream);
      break;
    case DSK_DNS_LOOKUP_RESULT_BAD_RESPONSE:
      stream_set_last_error (stream,
                             "dns lookup for %s failed: %s",
                             stream->name, result->message);
      maybe_set_autoreconnect_timer (stream);
      break;
    default:
      dsk_assert_not_reached ();
    }

  dsk_object_unref (stream);
}

static void
begin_connecting (DskClientStream *stream)
{
  if (stream->is_local_socket)
    {
      struct sockaddr_un addr;
      unsigned len = strlen (stream->name);
      if (len > sizeof (addr.sun_path))
        {
          /* name too long */
          /* TODO: catch this in constructor */
          stream_set_last_error (stream, "name too long for local socket");

          return;
        }
      addr.sun_family = AF_LOCAL;
      memcpy (addr.sun_path, stream->name,
              len == sizeof (addr.sun_path) ? len : len + 1);
      begin_connecting_sockaddr (stream, sizeof (addr), (struct sockaddr *) &addr);
    }
  else if (stream->is_numeric_name)
    {
      struct sockaddr_storage addr;
      unsigned addr_len;
      DskDnsAddress address;

      /* parse name into addr/addr_len */
      parse_numeric_ip (stream->name, &address);
      dns_address_to_sock_addr (&address, &addr, &addr_len);
      begin_connecting_sockaddr (stream, addr_len, (struct sockaddr *) &addr);
    }
  else
    {
      /* begin dns lookup */
      stream->is_resolving_name = 1;
      dsk_object_ref (stream);
      dsk_dns_lookup (stream->name, handle_dns_done, stream);
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
  int wrote;
  if (sink->fd < 0)
    {
      dsk_set_error (error, "no file-descriptor");
      return -1;
    }
  if (sink->is_connecting)
    {
      dsk_set_error (error, "file-descriptor %d not connected yet", sink->fd);
      return -1;
    }
  wrote = write (sink->fd, data_out, max_len);
  if (wrote < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        return 0;
      dsk_set_error (error, "error writing to client stream (fd %d): %s",
                     sink->fd, strerror (errno));
      return -1;
    }
  return wrote;
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

