
typedef struct _DskClientStreamSourceClass DskClientStreamSourceClass;
typedef struct _DskClientStreamSource DskClientStreamSource;
typedef struct _DskClientStreamSinkClass DskClientStreamSinkClass;
typedef struct _DskClientStreamSink DskClientStreamSink;
typedef struct _DskClientStreamClass DskClientStreamClass;
typedef struct _DskClientStream DskClientStream;


struct _DskClientStreamSourceClass
{
  DskOctetSourceClass base_class;
};
struct _DskClientStreamSource
{
  DskOctetSource base_instance;
  DskClientStream *owner;
};

struct _DskClientStreamSinkClass
{
  DskOctetSinkClass base_class;
};
struct _DskClientStreamSink
{
  DskOctetSink base_instance;
  DskClientStream *owner;
};

struct _DskClientStreamClass
{
  DskObjectClass base_class;
};
struct _DskClientStream
{
  DskObject base_instance;

  
  /* Hostname for normal (DNS-based) clients,
   * or path for local (ie unix-domain) clients.*/
  char *name;

  /* IP port on the given host.  actually a uint16 for IPv4 and IPv6;
     always 0 for local (unix-domain) clients. */
  unsigned port;

  /* The actual ip address (ipv4 or ipv6) that we are connecting to.
   * Only valid if !is_resolving_name. */
  DskIpAddress connect_addr;			/* if !is_resolving_name */

  /* An octet stream of data coming from the remote side.
     May be used directly. */
  DskClientStreamSource *source;

  /* An octet stream of data going to the remote side.
   * May be used directly. */
  DskClientStreamSink *sink;

  /* underlying file-descriptor */
  DskFileDescriptor fd;

  /* A hook that notifies when the stream is disconnected.
     May be used directly. */
  DskHook disconnect_hook;

  /* May be used directly. */
  DskHook connect_hook;

  /* Invoked whenever a new error occurs */
  DskHook error_hook;
  DskError *latest_error;

  /* for autoreconnect */
  DskDispatchTimer *reconnect_timer;
  int reconnect_time_ms;

  /* for idle-timeout */
  DskDispatchTimer *idle_disconnect_timer;
  int idle_disconnect_time_ms;

  /* private, mostly */
  unsigned char is_numeric_name : 1;
  unsigned char is_local_socket : 1;		/* ie unix-domain */
  unsigned char is_resolving_name : 1;
  unsigned char is_connecting : 1;
  unsigned char is_connected : 1;
  unsigned char shutdown_read : 1;
  unsigned char shutdown_write : 1;
};

extern DskClientStreamSourceClass dsk_client_stream_source_class;
extern DskClientStreamSinkClass dsk_client_stream_sink_class;
extern DskClientStreamClass dsk_client_stream_class;

typedef struct _DskClientStreamOptions DskClientStreamOptions;
struct _DskClientStreamOptions
{
  const char *hostname;
  unsigned port;
  DskIpAddress address;
  const char *path;          /* for unix-domain (aka local) sockets */
  int reconnect_time;        /* in milliseconds */
  int idle_disconnect_time;  /* in milliseconds */
};
#define DSK_CLIENT_STREAM_OPTIONS_DEFAULT \
{ NULL,                                     /* hostname */               \
  0,                                        /* port */                   \
  { 0, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} }, /* address */                \
  NULL,                                     /* path */                   \
  -1,                                       /* reconnect_time */         \
  -1,                                       /* idle_disconnect_time */   \
}

dsk_boolean      dsk_client_stream_new       (DskClientStreamOptions *options,
                                              DskClientStream **stream_out,
                                              DskOctetSource  **source_out,
                                              DskOctetSink    **sink_out,
                                              DskError        **error);

/* use -1 to disable these timeouts */
void             dsk_client_stream_set_reconnect_time (DskClientStream *client,
                                                       int              millis);
void             dsk_client_stream_set_max_idle_time  (DskClientStream *client,
                                                       int              millis);

void dsk_client_stream_disconnect (DskClientStream *stream);

