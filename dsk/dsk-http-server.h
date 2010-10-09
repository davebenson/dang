

typedef struct _DskHttpServerRequest DskHttpServerRequest;
typedef struct _DskHttpServerBindInfo DskHttpServerBindInfo;
typedef struct _DskHttpServer DskHttpServer;

typedef struct
{
  dsk_boolean is_get;           /* if !is_get, then its a POST CGI var */
  char *key;
  char *value;
  char *content_type;           /* for some POST headers */
} DskHttpCgiVar;

struct _DskHttpServerRequest
{
  DskHttpServerStreamTransfer *xfer;
  DskHttpRequest *request_header;
  DskHttpServerBindInfo *bind_info;

  dsk_boolean cgi_vars_computed;
  unsigned n_cgi_vars;
  DskHttpCgiVar *cgi_vars;

  dsk_boolean has_raw_post_data;
  size_t raw_post_data_size;
  uint8_t *raw_post_data;
};

typedef dsk_boolean (*DskHttpServerTestFunc)   (DskHttpServerRequest *request,
                                                void                 *func_data);
typedef void (*DskHttpServerStreamingPostFunc) (DskHttpServerRequest *request,
                                                DskOctetSource       *post_data,
                                                void                 *func_data);
typedef void (*DskHttpServerCGIFunc)           (DskHttpServerRequest *request,
                                                void                 *func_data);

/* MOST OF THESE FUNCTIONS CAN ONLY BE CALLED BEFORE THE SERVER IS STARTED */

typedef enum
{
  DSK_HTTP_SERVER_MATCH_PATH,
  DSK_HTTP_SERVER_MATCH_HOST,
  DSK_HTTP_SERVER_MATCH_USER_AGENT,
  DSK_HTTP_SERVER_MATCH_BIND_PORT,
  DSK_HTTP_SERVER_MATCH_BIND_PATH
} DskHttpServerMatchType;

void dsk_http_server_add_match                 (DskHttpServer        *server,
                                                DskHttpServerMatchType type,
                                                const char           *pattern);
void dsk_http_server_match_save                (DskHttpServer        *server);
void dsk_http_server_match_restore             (DskHttpServer        *server);

/* TODO */
//void dsk_http_server_match_add_auth          (DskHttpServer        *server,
                                                //????);
 
dsk_boolean dsk_http_server_bind_tcp           (DskHttpServer        *server,
                                                DskIpAddress         *bind_addr,
                                                unsigned              port,
                                                DskError            **error);
dsk_boolean dsk_http_server_bind_local         (DskHttpServer        *server,
                                                const char           *path,
                                                DskError            **error);


void
dsk_http_server_register_streaming_post_handler (DskHttpServer *server,
                                                 DskHttpServerStreamingPostFunc func,
                                                 void          *func_data,
                                                 DskHookDestroy destroy);

void
dsk_http_server_register_streaming_cgi          (DskHttpServer *server,
                                                 DskHttpServerCGIFunc func,
                                                 void          *func_data,
                                                 DskHookDestroy destroy);


/* One of these functions should be called by any handler */
void dsk_http_server_request_respond_stream   (DskHttpServerRequest *request,
                                               DskOctetSource       *source);
void dsk_http_server_request_respond_data     (DskHttpServerRequest *request,
                                               size_t                length,
                                               const uint8_t        *data);
void dsk_http_server_request_respond_file     (DskHttpServerRequest *request,
                                               const char           *filename);
void dsk_http_server_request_respond_error    (DskHttpServerRequest *request,
                                               DskHttpStatus         status,
                                               const char           *message);
void dsk_http_server_request_redirect         (DskHttpServerRequest *request,
                                               DskHttpStatus         status,
                                               const char           *location);
void dsk_http_server_request_internal_redirect(DskHttpServerRequest *request,
                                               const char           *new_path);
void dsk_http_server_request_pass             (DskHttpServerRequest *request);

/* --- used to configure our response a little --- */
void dsk_http_server_request_set_content_type (DskHttpServerRequest *request,
                                               const char           *triple);
void dsk_http_server_request_set_content_type_parts
                                              (DskHttpServerRequest *request,
                                               const char           *type,
                                               const char           *subtype,
                                               const char           *charset);
