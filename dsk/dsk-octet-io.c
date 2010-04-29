#include "dsk.h"

static void
dsk_octet_source_init (DskOctetSource *source)
{
  dsk_hook_init (&source->readable_hook, source);
}

static void
dsk_octet_source_finalize (DskOctetSource *source)
{
  if (!source->readable_hook.is_cleared)
    dsk_hook_clear (&source->readable_hook);
}

static void
dsk_octet_sink_init (DskOctetSink *sink)
{
  dsk_hook_init (&sink->writable_hook, sink);
}

static void
dsk_octet_sink_finalize (DskOctetSink *sink)
{
  if (!sink->writable_hook.is_cleared)
    dsk_hook_clear (&sink->writable_hook);
}

DSK_OBJECT_CLASS_DEFINE_CACHE_DATA (DskOctetSink);
DskOctetSinkClass dsk_octet_sink_class =
{
  DSK_OBJECT_CLASS_DEFINE (DskOctetSink, &dsk_object_class, 
                           dsk_octet_sink_init,
                           dsk_octet_sink_finalize),
  NULL,                 /* no default write impl */
  NULL,                 /* no default write_buffer impl */
  NULL,                 /* no default shutdown impl */
};

DSK_OBJECT_CLASS_DEFINE_CACHE_DATA (DskOctetSource);
DskOctetSourceClass dsk_octet_source_class =
{
  DSK_OBJECT_CLASS_DEFINE (DskOctetSource, &dsk_object_class, 
                           dsk_octet_source_init,
                           dsk_octet_source_finalize),
  NULL,                 /* no default read impl */
  NULL,                 /* no default read_buffer impl */
  NULL,                 /* no default shutdown impl */
};

