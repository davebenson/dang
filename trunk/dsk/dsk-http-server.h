

typedef void (*DskHttpServerTestFunc)          (DskHttpServerRequest *request,
                                                void                 *func_data);
typedef void (*DskHttpServerStreamingPostFunc) (DskHttpServerRequest *request,
                                                DskOctetSource       *post_data,
                                                void                 *func_data);


void dsk_http_server_matcher_add_path_prefix (...);
void dsk_http_server_matcher_add_path (...);
void dsk_http_server_matcher_add_user_agent_pattern (...);
void dsk_http_server_matcher_add_host (...);
void dsk_http_server_matcher_add_test (...);
void dsk_http_server_matcher_add_auth (...);
void dsk_http_server_matcher_save (...);
void dsk_http_server_matcher_restore (...);


void dsk_http_server_register_streaming_post_handler (DskHttpServer *server,
                                                      DskHttpServerMatcher *location,
                                                      DskHttpServerStreamingPostFunc func,
                                                      void *func_data);


/* One of these functions should be called by any handler */
void dsk_http_server_request_respond (...);
void dsk_http_server_request_respond_data (...);
void dsk_http_server_request_respond_file (...);
void dsk_http_server_request_respond_error (...);
void dsk_http_server_request_redirect (...);
void dsk_http_server_request_pass (...);
