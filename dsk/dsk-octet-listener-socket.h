
typedef struct _DskOctetListenerSocketClass DskOctetListenerSocketClass;
typedef struct _DskOctetListenerSocket DskOctetListenerSocket;

struct _DskOctetListenerSocketClass
{
  DskOctetListenerClass base_class;
};
struct _DskOctetListenerSocket
{
  DskOctetListener base_instance;

  dsk_boolean is_local;

  /* for local (unix-domain) sockets */
  char *path;

  /* for TCP/IP sockets */
  DskIpAddress bind_address;
  dsk_boolean bind_port;
  
  /* the underlying listening file descriptor */
  DskFileDescriptor listening_fd;
};

extern const DskOctetListenerSocketClass dsk_octet_listener_socket_class;
#define DSK_OCTET_LISTENER_SOCKET(object) DSK_OBJECT_CAST(DskOctetListenerSocket, object, &dsk_octet_listener_socket_class)
