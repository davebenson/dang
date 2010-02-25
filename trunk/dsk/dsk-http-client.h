
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

void dsk_http_client_request       (DskHttpClient *client,
                                    DskHttpRequest *request,
				    DskOctetSource *post_data,
				    DskHttpClientRequestOptions *options,
				    DskHttpClientRequestFuncs *func,
				    void              *func_data,
				    DskDestroyNotify   destroy);


void dsk_http_client_add_host_pool (DskHttpClient *client,
                                    const char    *hostname,
				    unsigned       port,
				    unsigned       desired_pool_size);

