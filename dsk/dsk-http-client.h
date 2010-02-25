
typedef struct _DskHttpClient DskHttpClient;

void dsk_http_client_request       (DskHttpClient *client,
                                    DskHttpRequest *request,
				    DskOctetSource *post_data,
				    DskHttpRequestOptions *options,
				    DskHttpRequestFunc func,
				    void              *func_data,
				    DskDestroyNotify   destroy);


void dsk_http_client_add_host_pool (DskHttpClient *client,
                                    const char    *hostname,
				    unsigned       port,
				    unsigned       desired_pool_size);

