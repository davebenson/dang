#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include "dsk-common.h"
#include "dsk-mem-pool.h"
#include "dsk-hook.h"
#include "dsk-object.h"
#include "dsk-error.h"
#include "dsk-buffer.h"
#include "dsk-octet-io.h"
#include "dsk-octet-listener.h"
#include "dsk-ip-address.h"
#include "dsk-fd.h"
#include "dsk-octet-fd.h"
#include "dsk-octet-listener-socket.h"

static DskIOResult
dsk_octet_listener_socket_accept (DskOctetListener        *listener,
                                  DskOctetStream         **stream_out,
			          DskOctetSource         **source_out,
			          DskOctetSink           **sink_out,
                                  DskError               **error)
{
  DskOctetListenerSocket *s = DSK_OCTET_LISTENER_SOCKET (listener);
  struct sockaddr addr;
  socklen_t addr_len;
  DskFileDescriptor fd = accept (s->listening_fd, &addr, &addr_len);
  if (fd < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        return DSK_IO_RESULT_AGAIN;
      dsk_set_error (error, "error accepting connection from listening fd %d: %s",
                     (int) s->listening_fd, strerror (errno));
      return DSK_IO_RESULT_ERROR;
    }

  if (!dsk_octet_stream_new_fd (fd, DSK_FILE_DESCRIPTOR_IS_READABLE|
                                    DSK_FILE_DESCRIPTOR_IS_WRITABLE|
                                    DSK_FILE_DESCRIPTOR_IS_POLLABLE,
                                    (DskOctetStreamFd **) stream_out,
                                    (DskOctetStreamFdSource **) source_out,
                                    (DskOctetStreamFdSink **) sink_out,
                                    error))
    {
      close (fd);
      return DSK_IO_RESULT_ERROR;
    }
  return DSK_IO_RESULT_SUCCESS;
}

static void
dsk_octet_listener_socket_init (DskOctetListenerSocket *s)
{
  s->listening_fd = -1;
}
static void
dsk_octet_listener_socket_finalize (DskOctetListenerSocket *s)
{
  if (s->listening_fd >= 0)
    {
      close (s->listening_fd);
      s->listening_fd = -1;
    }
  dsk_free (s->path);
  dsk_free (s->bind_iface);
}

DSK_OBJECT_CLASS_DEFINE_CACHE_DATA(DskOctetListenerSocket);
const DskOctetListenerSocketClass dsk_octet_listener_socket_class =
{ { DSK_OBJECT_CLASS_DEFINE(DskOctetListenerSocket,
                            &dsk_octet_listener_class,
                            dsk_octet_listener_socket_init,
                            dsk_octet_listener_socket_finalize),
    dsk_octet_listener_socket_accept
} };

