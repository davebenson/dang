#include "dsk.h"

DSK_OBJECT_CLASS_DEFINE_CACHE_DATA(DskOctetFilter);
const DskOctetFilterClass dsk_octet_filter_class =
{
  DSK_OBJECT_CLASS_DEFINE (DskOctetFilter, &dsk_object_class, NULL, NULL),
  NULL, NULL
};
