typedef struct _DskUdpSocketClass DskUdpSocketClass;
typedef struct _DskUdpSocket DskUdpSocket;


struct _DskUdpSocketClass
{
  DskObjectClass base_class;
};
struct _DskUdpSocket
{
  DskObject base_instance;
  DskHook readable;
  DskHook writable;

  unsigned is_bound : 1;
  unsigned is_connected : 1;
  unsigned is_ipv6 : 1;

  DskDnsAddress bound_address;
  DskDnsAddress connect_address;
};

DskUdpSocket * dsk_udp_socket_new     (dsk_boolean  is_ipv6,
                                       DskError   **error);
dsk_boolean    dsk_udp_socket_send    (DskUdpSocket  *socket,
			               DskError     **error);
dsk_boolean    dsk_udp_socket_send_to (DskUdpSocket  *socket,
                                       const char    *name,
		                       unsigned       port,
			               unsigned       len,
			               const uint8_t *data,
			               DskError     **error);
dsk_boolean    dsk_udp_socket_bind    (DskUdpSocket  *socket,
                                       DskDnsAddress *bind_addr,
				       unsigned       port,
			               DskError     **error);
dsk_boolean    dsk_udp_socket_receive (DskUdpSocket  *socket,
                                       DskDnsAddress *addr_out,
			               unsigned      *port_out,
			               unsigned      *len_out,
			               uint8_t      **data_out,
			               DskError     **error);

