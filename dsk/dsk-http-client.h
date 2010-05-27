
typedef struct _DskHttpClient DskHttpClient;
typedef struct _DskHttpClientClass DskHttpClientClass;

typedef struct _DskHttpClientRequestOptions DskHttpClientRequestOptions;
struct _DskHttpClientRequestOptions
{
  unsigned max_redirects;
};

typedef DskHttpClientStreamTransfer DskHttpClientTransfer;

typedef struct _DskHttpClientRequestFuncs DskHttpClientRequestFuncs;
struct _DskHttpClientRequestFuncs
{
  void (*handle_response) (DskHttpClientTransfer *xfer,
                           void                  *func_data);
};

struct _DskHttpClientClass
{
  DskObjectClass base_class;
};
struct _DskHttpClient
{
  DskObject base_instance;
};

typedef struct _DskHttpClientOptions DskHttpClientOptions;
struct _DskHttpClientOptions
{
  unsigned max_connections_keptalive;
  unsigned max_connections_per_host_keptalive;
};

/* TODO: provide some sort of evidence that these are good numbers */
#define DSK_HTTP_CLIENT_OPTIONS_DEFAULT               \
{                                                     \
  100,      /* max_connections_keptalive */           \
  5         /* max_connections_per_host_keptalive */  \
}


DskHttpClient *dsk_http_client_new (DskHttpClientOptions *options);

struct _DskHttpClientRequestOptions
{
  /* Select location */
  /* by URL ... */
  char *url;

  /* ... or by URL pieces */
  char *host;
  unsigned port;                /* 0 = unspecified */
  char *path;
  char *query;

  /* May we attempt to pipeline this request? (default: yes) */
  dsk_boolean pipeline;

  /* Number of milliseconds to keepalive this connection */
  int keepalive_millis;

  /* Force connection-close */
  dsk_boolean connection_close;

  /* Allow Content-Encoding gzip (will be TRUE once supported) */
  dsk_boolean allow_gzip;

  /* Force the POST data to be gzipped. */
  dsk_boolean gzip_post_data;

  /* TODO: authentication support */
  /* TODO: proxy authentication support */
  /* TODO: provide POST-data MD5Sum */
  /* TODO: retry support */
  /* TODO: MD5Sum checking support */
  /* TODO: POST-data CGI variables? */
  /* TODO: GET CGI variables? */
  /* TODO: max memory/disk or streaming */
  /* TODO: timeout[s] (wget provides 'dns_timeout') */
  /* TODO: ipv4 or ipv6? (options: ipv4,ipv6,system) */
  /* TODO: option to request server not to use cache (--no-cache in wget) */
  /* TODO: cookie support */
  /* TODO: way to send Basic-Auth preemptively (w/o "challenge") */

  /* TODO TODO: SSL options (for HTTPS obviously..) */
};

void dsk_http_client_request       (DskHttpClient               *client,
				    DskHttpClientRequestOptions *options,
				    DskOctetSource              *post_data,
				    DskHttpClientRequestFuncs   *funcs,
				    void                        *func_data,
				    DskDestroyNotify             destroy);
