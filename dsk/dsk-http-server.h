

typedef struct _DskHttpServerRequest DskHttpServerRequest;
typedef struct _DskHttpServerMatcher DskHttpServerMatcher;
typedef struct _DskHttpServer DskHttpServer;


typedef dsk_boolean (*DskHttpServerTestFunc)   (DskHttpServerRequest *request,
                                                void                 *func_data);
typedef void (*DskHttpServerStreamingPostFunc) (DskHttpServerRequest *request,
                                                DskOctetSource       *post_data,
                                                void                 *func_data);

/* MOST OF THESE FUNCTIONS CAN ONLY BE CALLED BEFORE THE SERVER IS STARTED */


void dsk_http_server_match_path_prefix         (DskHttpServer        *server,
                                                const char           *prefix);
void dsk_http_server_match_path_suffix         (DskHttpServer        *server,
                                                const char           *suffix);
void dsk_http_server_match_path                (DskHttpServer        *server,
                                                const char           *suffix);
void dsk_http_server_match_user_agent_pattern  (DskHttpServer        *server,
                                                const char           *pattern);
void dsk_http_server_match_host                (DskHttpServer        *server,
                                                const char           *host);
void dsk_http_server_match_save                (DskHttpServer        *server);
void dsk_http_server_match_restore             (DskHttpServer        *server);

/* TODO */
//void dsk_http_server_match_add_auth          (DskHttpServer        *server,
                                                //????);
 


void
dsk_http_server_register_streaming_post_handler (DskHttpServer *server,
                                                 DskHttpServerMatcher *location,
                                                 DskHttpServerStreamingPostFunc func,
                                                 void *func_data);


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
