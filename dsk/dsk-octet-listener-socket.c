#include "dsk-common.h"
#include "dsk-mem-pool.h"
#include "dsk-hook.h"
#include "dsk-object.h"
#include "dsk-error.h"
#include "dsk-buffer.h"
#include "dsk-octet-io.h"
#include "dsk-octet-listener.h"
#include "dsk-octet-listener-socket.h"

static DskIOResult
dsk_octet_listener_socket_accept (DskOctetListener        *listener,
                                  DskOctetStream         **stream_out,
			          DskOctetSource         **source_out,
			          DskOctetSink           **sink_out,
                                  DskError               **error)
{
  ...
}

DSK_OBJECT_CLASS_DEFINE_CACHE_DATA(DskOctetListenerSocket);
const DskOctetListenerSocketClass dsk_octet_listener_socket_class =
{ { DSK_OBJECT_CLASS_DEFINE(DskOctetListenerSocket,
                            dsk_octet_listener_socket_init,
                            dsk_octet_listener_socket_finalize)
    dsk_octet_listener_socket_accept
} };

