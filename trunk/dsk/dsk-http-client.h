
typedef struct _DskHttpClient DskHttpClient;
typedef struct _DskHttpClientClass DskHttpClientClass;
typedef struct _DskHttpClientRequestOptions DskHttpClientRequestOptions;

typedef DskHttpClientStreamTransfer DskHttpClientTransfer;

typedef struct _DskHttpClientRequestFuncs DskHttpClientRequestFuncs;
struct _DskHttpClientRequestFuncs
{
  /* handle_done/handle_fail:
     one of these two functions is always invoked once. */

  /* handle_done:
   * All the response header and body have been downloaded
     and processed successfully.
   */
  void (*handle_done)     (DskHttpClientTransfer *xfer,
                           void                  *func_data);

  /* handle_fail:
   * An error -- or many errors -- have caused the endeavor
   * to receive a successful response to fail.
   *
   * We will failures to connect, retry 504 responses
   * and all things that look like network noise (bad MD5,
   * cut-off in the middle of content).
   * But there is a maximum number of retries.
   *
   * And things like 404 headers fail after one try.
   *
   * And requests other than HEAD and GET are not retried.
   * (NOT FINAL)
   */
  void (*handle_fail)     (DskHttpClientTransfer *xfer,
                           DskError              *error,
                           void                  *func_data);

  /* --- low-level notifications (called before done/fail) --- */
  void (*handle_response)    (DskHttpClientTransfer *xfer,
                              void                  *func_data);

  void (*handle_redirecting) (DskHttpClientTransfer *xfer,
                              void                  *func_data);

  void (*handle_retrying)    (DskHttpClientTransfer *xfer,
                              void                  *func_data);

  /* error: this may or may not be fatal; use the handle_fail()
     to trap terminate errors */
  void (*handle_error)       (DskHttpClientTransfer *xfer,
                              DskError              *error,
                              void                  *func_data);


  /* called after handle_{done,fail} */
  void (*handle_destroy)  (DskHttpClientTransfer *xfer,
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
  unsigned max_connections_total;

  /* logging options?  or maybe a "trap" system that can be used for it? */
};

/* TODO: provide some sort of evidence that these are good numbers */
#define DSK_HTTP_CLIENT_OPTIONS_DEFAULT               \
{                                                     \
  100,      /* max_connections_keptalive */           \
  5,        /* max_connections_per_host_keptalive */  \
  10000000  /* max_connections_total */               \
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

  /* hint to skip DNS lookups, give a direct address instead */
  dsk_boolean has_ip_address;
  DskIpAddress ip_address;

  /* use a local socket (aka a unix-domain socket) 
     instead of the actual IP address/dns lookup.
     This is mostly for testing. */
  char *local_socket_path;

  /* TODO: POST-data CGI variables? */

  /* GET CGI variables: will be added to query string if it exists,
     and a query string (starting with a '?') will be added otherwise. */
  char **extra_get_cgi_variables;

  /* May we attempt to pipeline this request? (default: yes for GET/HEAD) */
  unsigned pipeline_head : 1;
  unsigned pipeline_get : 1;
  unsigned pipeline_post : 1;
  unsigned pipeline_put : 1;
  unsigned pipeline_delete : 1;

  /* overrides */
  unsigned pipeline : 1;    /* equivalent to setting all pipeline flags */
  unsigned no_pipeline : 1; /* equivalent to unsetting all pipeline flags */
                                                    

  /* Number of milliseconds to keepalive this connection */
  int keepalive_millis;

  /* Force connection-close */
  dsk_boolean connection_close;

  /* Allow Content-Encoding gzip (will be TRUE once supported) */
  dsk_boolean allow_gzip;

  /* Force the POST data to be gzipped. */
  dsk_boolean gzip_post_data;

#if 0
  /* TODO: authentication support */
  /* TODO: way to send Basic-Auth preemptively (w/o "challenge") */
  DskHttpClientAuth *auth_agent;
#endif

  /* Provide POST-data MD5Sum */
  dsk_boolean has_postdata_md5sum;
  uint8_t postdata_md5sum_binary[16];
  char *postdata_md5sum_hex;

  /* Retry support */
  int max_retries;
  unsigned retry_sleep_millis;

  /* Redirect support */
  unsigned max_redirects;

  /* MD5Sum checking support */
  dsk_boolean check_md5sum;

  /* --- timeouts --- */

  /* max for a single HTTP request (from the DNS lookup starting
     to the response finishing) */
  int max_request_time_millis;

  /* max time for the content download to start */
  int max_start_millis;

  /* max time for the content download to finish */
  int max_millis;

  /* TODO: max memory/disk or streaming */
  

  /* TODO: option to request server not to use cache (--no-cache in wget) */

  /* Cookies to send */
  unsigned n_cookies;
  DskHttpCookie *cookies;

  /* TODO TODO: SSL options (for HTTPS obviously..) */
};

void dsk_http_client_request       (DskHttpClient               *client,
				    DskHttpClientRequestOptions *options,
				    DskOctetSource              *post_data,
				    DskHttpClientRequestFuncs   *funcs,
				    void                        *func_data,
				    DskDestroyNotify             destroy);

