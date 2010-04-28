#include "dsk.h"

static DskOctetConnectionOptions default_options = DSK_OCTET_CONNECTION_OPTIONS_DEFAULT;

static dsk_boolean
handle_source_readable (void       *object,
                        void       *callback_data)
{
  DskOctetSource *source = object;
  DskOctetConnection *conn = callback_data;
  DskError *error = NULL;
  dsk_assert (conn->source == source);

  switch (dsk_octet_source_read_buffer (source, &conn->buffer, &error))
    {
    case DSK_IO_RESULT_SUCCESS:
      break;
    case DSK_IO_RESULT_AGAIN:
      return DSK_TRUE;
    case DSK_IO_RESULT_EOF:            /* only for read operations */
      goto done_reading;
    case DSK_IO_RESULT_ERROR:
      if (conn->shutdown_on_write_error)
        dsk_octet_source_shutdown (source);
      goto done_reading;
    }
  if (conn->buffer.size > 0
   && conn->write_trap->block_count == 1)
    dsk_hook_trap_unblock (conn->write_trap);
  return DSK_TRUE;

done_reading:
  {
    DskHookTrap *read_trap = conn->read_trap;
    dsk_object_unref (conn->source);
    conn->source = NULL;
    conn->read_trap = NULL;
    if (conn->buffer.size == 0)
      {
        DskHookTrap *write_trap = conn->write_trap;
        if (write_trap)
          {
            conn->write_trap = NULL;
            if (write_trap->block_count > 0)
              dsk_hook_trap_unblock (write_trap);
            if (conn->sink)
              dsk_octet_sink_shutdown (conn->sink);
            dsk_hook_trap_destroy (write_trap);
          }
      }
    dsk_hook_trap_destroy (read_trap);
  }
  return DSK_FALSE;
}
static dsk_boolean
handle_sink_writable (void *object, void *callback_data)
{
  DskOctetConnection *conn = callback_data;
  DskOctetSink *sink = object;
  DskError *error = NULL;
  dsk_assert (conn->sink == sink);
  switch (dsk_octet_sink_write_buffer (sink, &conn->buffer, &error))
    {
    case DSK_IO_RESULT_SUCCESS:
    case DSK_IO_RESULT_AGAIN:
      break;
    case DSK_IO_RESULT_EOF:
      dsk_assert_not_reached ();
    case DSK_IO_RESULT_ERROR:
      goto got_error;
    }
  if (conn->buffer.size == 0 && conn->write_trap->block_count == 0)
    dsk_hook_trap_block (conn->write_trap);
  return DSK_TRUE;

got_error:
  if (conn->shutdown_on_write_error)
    {
      ...
    }
  ...
}

DskOctetConnection *
dsk_octet_connection_new (DskOctetSource *source,
                          DskOctetSink   *sink,
                          DskOctetConnectionOptions *opt)
{
  DskOctetConnection *connection;
  dsk_assert (dsk_object_is_a (source, &dsk_octet_source_class));
  dsk_assert (dsk_object_is_a (sink, &dsk_octet_sink_class));
  if (opt == NULL)
    opt = &default_options;
  connection = dsk_object_new (&dsk_octet_connection_class);
  connection->source = dsk_object_ref (source);
  connection->sink = dsk_object_ref (sink);
  connection->max_buffer = opt->max_buffer;
  connection->shutdown_on_read_error = opt->shutdown_on_read_error;
  connection->shutdown_on_write_error = opt->shutdown_on_write_error;
  connection->read_trap = dsk_hook_trap (&(source->readable_hook),
                                         handle_source_readable,
                                         dsk_object_ref (connection),
                                         (DskHookDestroy) dsk_object_unref_f);
  connection->write_trap = dsk_hook_trap (&(sink->writable_hook),
                                          handle_sink_writable,
                                          dsk_object_ref (connection),
                                          (DskHookDestroy) dsk_object_unref_f);
  dsk_hook_trap_block (connection->write_trap);
  return connection;
}

void                dsk_octet_connection_shutdown (DskOctetConnection *);
void                dsk_octet_connection_disconnect (DskOctetConnection *);


DskOctetConnectionClass dsk_octet_connection_class =
{
  DSK_OBJECT_CLASS_DEFINE (DskOctetConnection,
                           &dsk_object_class,
                           NULL,
                           dsk_octet_connection_finalize)
};
