#include "dsk.h"

static DskOctetConnectionOptions default_options = DSK_OCTET_CONNECTION_OPTIONS_DEFAULT;

DskOctetConnection *
dsk_octet_connection_new (DskOctetSource *source,
                          DskOctetSink   *sink,
                          DskOctetConnectionOptions *opt)
{
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

  ...
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
