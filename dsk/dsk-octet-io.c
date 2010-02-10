#include "dsk.h"

static void
dsk_octet_source_init (DskOctetSource *source)
{
  source->readable_hook = dsk_hook_new (source);
}

static void
dsk_octet_source_finalize (DskOctetSource *source)
{
  if (source->readable_hook)
    {
      dsk_hook_destroy (source->readable_hook);
      source->readable_hook = NULL;
    }
}

static void
dsk_octet_sink_init (DskOctetSink *sink)
{
  sink->writable_hook = dsk_hook_new (sink);
}

static void
dsk_octet_sink_finalize (DskOctetSink *sink)
{
  if (sink->writable_hook)
    {
      dsk_hook_destroy (sink->writable_hook);
      sink->writable_hook = NULL;
    }
}

DskOctetSourceClass dsk_octet_sink_class =
{
  DSK_OBJECT_CLASS_DEFINE (DskOctetSink, &dsk_object_class, 
                           dsk_octet_sink_init,
                           dsk_octet_sink_finalize),
  NULL,                 /* no default write impl */
  NULL,                 /* no default write_buffer impl */
  NULL,                 /* no default shutdown impl */
};

DskObjectSourceClass dsk_octet_source_class =
{
  DSK_OBJECT_CLASS_DEFINE (DskOctetSource, &dsk_object_class, 
                           dsk_octet_source_init,
                           dsk_octet_source_finalize),
  NULL,                 /* no default read impl */
  NULL,                 /* no default read_buffer impl */
  NULL,                 /* no default shutdown impl */
};
