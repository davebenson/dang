#include "dsk.h"

DskOctetSourceClass dsk_octet_sink_class =
{
  DSK_OBJECT_CLASS_DEFINE (DskOctetSink, &dsk_object_class, NULL),
  NULL,                 /* no default write impl */
  NULL,                 /* no default write_buffer impl */
  NULL,                 /* no default shutdown impl */
};

DskObjectSourceClass dsk_octet_source_class =
{
  DSK_OBJECT_CLASS_DEFINE (DskOctetSource, &dsk_object_class, NULL),
  NULL,                 /* no default read impl */
  NULL,                 /* no default read_buffer impl */
  NULL,                 /* no default shutdown impl */
};
