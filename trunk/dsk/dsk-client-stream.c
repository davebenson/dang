#include "dsk.h"

DskClientStreamClass dsk_client_stream_class =
{
  DSK_OBJECT_CLASS_DEFINE(DskClientStream, &dsk_object_class,
                          NULL, dsk_client_stream_finalize)
};


DskClientStreamSinkClass dsk_client_stream_source_class =
{
  { DSK_OBJECT_CLASS_DEFINE (DskClientStreamSource, &dsk_octet_source_class,
                             NULL, NULL),
    dsk_client_stream_source_read,
    dsk_client_stream_source_read_buffer,
    dsk_client_stream_source_shutdown,
  }
};
DskClientStreamSinkClass dsk_client_stream_sink_class =
{
  { DSK_OBJECT_CLASS_DEFINE (DskClientStreamSink, &dsk_octet_sink_class,
                             NULL, NULL),
    dsk_client_stream_sink_write,
    dsk_client_stream_sink_write_buffer,
    dsk_client_stream_sink_shutdown,
  }
};

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

static void
begin_connecting_dns_entry (DskClientStream *stream,
                            DskDnsAddress   *address,
                            unsigned         port)
{
  stream->is_connecting = 1;
  ...
}

DskClientStream *dsk_client_stream_new       (const char *name,
                                              unsigned    port,
                                              DskError  **error)
{
  DskClientStream *rv;
  if (dsk_hostname_looks_numeric (name))
    {
      /* if name is numeric begin connecting */
      DskDnsAddress addr;
      if (!dsk_dns_address_parse (name, &addr, error))
        return NULL;
      rv = create_raw_client_stream ();
      rv->is_numeric_name = 1;
      if (!begin_connecting_dns_entry (rv, &addr, port, error))
        {
          dsk_object_unref (rv);
          return NULL;
        }
    }
  else
    {
      /* begin name resolution */
      rv = create_raw_client_stream ();
      ...
    }
}

DskClientStream *dsk_client_stream_new_addr  (DskDnsAddress *addr,
                                              unsigned       port,
                                              DskError     **error)
{
  DskClientStream *rv = create_raw_client_stream ();
  rv->name = dsk_dns_address_to_string (addr);
  rv->port = port;
  begin_connecting_dns_entry (rv, addr, port);
  return rv;
}

DskClientStream *dsk_client_stream_new_local (const char *path)
{
  DskClientStream *rv = create_raw_client_stream ();
  rv->is_numeric_name = 1;
  ...
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
