
typedef struct _DskClientStreamSource DskClientStreamSource;
typedef struct _DskClientStreamSink DskClientStreamSink;
typedef struct _DskClientStream DskClientStream;


struct _DskClientStreamSource
{
  DskOctetSource base_instance;
  DskClientStream *owner;
};

struct _DskClientStreamSink
{
  DskOctetSink base_instance;
  DskClientStream *owner;
};

struct _DskClientStream
{
  DskObject base_instance;

  
  /* Hostname for normal (DNS-based) clients,
   * or path for local (ie unix-domain) clients.*/
  char *name;
  `
  /* IP port on the given host.  actually a uint16 for IPv4 and IPv6;
     always 0 for local (unix-domain) clients. */
  unsigned port;

  /* The actual ip address (ipv4 or ipv6) that we are connecting to.
   * Only valid if !is_resolving_name. */
  DskDnsAddress connect_addr;			/* if !is_resolving_name */

  /* An octet stream of data coming from the remote side.
     May be used directly. */
  DskClientStreamSource *source;

  /* An octet stream of data going to the remote side.
   * May be used directly. */
  DskClientStreamSink *sink;

  /* A hook that notifies when the stream is disconnected.
     May be used directly. */
  DskHook *disconnect_hook;

  /* May be used directly. */
  DskHook *connect_hook;

  /* for autoreconnect */
  DskDispatchTimer *reconnect_timer;
  int reconnect_time_ms;

  /* for idle-timeout */
  DskDispatchTimer *idle_disconnect_timer;
  int max_idle_time_ms;

  /* private, mostly */
  unsigned char is_numeric_name : 1;
  unsigned char is_local_socket : 1;		/* ie unix-domain */
  unsigned char is_resolving_name : 1;
  unsigned char is_connecting : 1;
  unsigned char is_connected : 1;
};

extern DskClientStreamSourceClass dsk_client_stream_source_class;
extern DskClientStreamSinkClass dsk_client_stream_sink_class;
extern DskClientStreamClass dsk_client_stream_class;

DskClientStream *dsk_client_stream_new       (const char *name,
                                              unsigned    port);
DskClientStream *dsk_client_stream_new_addr  (DskDnsAddress *addr,
                                              unsigned    port);
DskClientStream *dsk_client_stream_new_local (const char *path);

/* use -1 to disable these timeouts */
void             dsk_client_stream_set_reconnect_time (DskClientStream *client,
                                                       int              millis);
void             dsk_client_stream_set_max_idle_time  (DskClientStream *client,
                                                       int              millis);


void             dsk_client_stream_set_max_idle_time  (DskClientStream *client,
