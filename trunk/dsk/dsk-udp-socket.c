#include "dsk-common.h"
#include "dsk-hook.h"
#include "dsk-object.h"
#include "dsk-dns-client.h"

DskUdpSocket *
dsk_udp_socket_new     (dsk_boolean  is_ipv6,
                        DskError   **error)
{
  DskFileDescriptor fd;
  int domain = AF_INET;
  DskUdpSocket *rv;
  if (is_ipv6)
    {
#if HAVE_IPV6
      domain = AF_INET6;
#else
      dsk_set_error (error, "IPv6 not supported");
      return NULL;
#endif
    }
  fd = socket (domain, SOCK_DGRAM, 0);
  if (fd < 0)
    {
      int e = errno;
      dsk_fd_creation_failed (e);
      dsk_set_error (error, "error creating file-descriptor: %s",
                     strerror (e));
      return NULL;
    }
  rv = dsk_object_new (&dsk_udp_socket_class);
  rv->fd = fd;
  dsk_fd_set_nonblocking (fd);
  return rv;
}

DskIOResult
dsk_udp_socket_send    (DskUdpSocket  *socket,
                        unsigned       len,
                        const uint8_t *data,
                        DskError     **error)
{
  ssize_t rv;
  if (!socket->is_connected)
    {
      dsk_set_error (error, "cannot send udp packet without being connected");
      return DSK_IO_RESULT_ERROR;
    }
  rv = send (socket->fd, data, len, 0);
  if (rv < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        return DSK_IO_RESULT_AGAIN;
      dsk_set_error ("error sending udp packet: %s", strerror (errno));
      return DSK_IO_RESULT_ERROR;
    }
  if ((size_t) rv < len)
    {
      dsk_set_error ("data truncated sending udp packet (%u of %u bytes sent)",
                     (unsigned) rv, (unsigned) len);
      return DSK_IO_RESULT_ERROR;
    }
  return DSK_IO_RESULT_SUCCESS;
}

DskIOResult
dsk_udp_socket_send_to (DskUdpSocket  *socket,
                        const char    *name,
                        unsigned       port,
                        unsigned       len,
                        const uint8_t *data,
                        DskError     **error)
{
  ...
}

dsk_boolean
dsk_udp_socket_bind    (DskUdpSocket  *socket,
                        DskDnsAddress *bind_addr,
                        unsigned       port,
                        DskError     **error)
{
  ...
}

DskIOResult
dsk_udp_socket_receive (DskUdpSocket  *socket,
                        DskDnsAddress *addr_out,
                        unsigned      *port_out,
                        unsigned      *len_out,
                        uint8_t      **data_out,
                        DskError     **error)
{
  ...
}
