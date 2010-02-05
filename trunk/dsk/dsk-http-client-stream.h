typedef struct _DskHttpClientStreamClass DskHttpClientStreamClass;
typedef struct _DskHttpClientStream DskHttpClientStream;
typedef struct _DskHttpClientStreamFuncs DskHttpClientStreamFuncs;
typedef struct _DskHttpClientStreamTransfer DskHttpClientStreamTransfer;

struct _DskHttpClientStreamClass
{
  DskObject base_instance;
};
struct _DskHttpClientStream
{
  DskObject base_instance;
  DskOctetSink *sink;
  DskOctetSource *source;
  DskHttpClientStreamTransfer *first_transfer, *last_transfer;
};

/* internals */
struct _DskHttpClientStreamTransfer
{
  DskHttpClientStream *owner;
  DskHttpRequest *request;
  DskHttpResponse *response;
  DskMemoryBufferSource *content;      
  DskHttpClientStreamTransfer *next;
  DskHttpClientStreamFuncs *funcs;
  void *user_data;
};

DskHttpClientStream *
dsk_http_client_stream_new     (DskOctetSink        *sink,
                                DskOctetSource      *source);

typedef enum
{
  DSK_HTTP_CLIENT_STREAM_ERROR_PREMATURE_SHUTDOWN,
  DSK_HTTP_CLIENT_STREAM_ERROR_BAD_HEADER,
  DSK_HTTP_CLIENT_STREAM_ERROR_BAD_CONTENT,
} DskHttpClientStreamError;

struct _DskHttpClientStreamFuncs
{
  /* called once the http header is received.
     content may or may not be complete. */
  void (*handle_response)         (DskHttpClientStreamTransfer *transfer);

  /* called once content is completely received */
  void (*handle_content_complete) (DskHttpClientStreamTransfer *transfer);

  /* call for any number of errors */
  void (*handle_error)            (DskHttpClientStreamTransfer *transfer);

  /* always called exactly once after all other functions done */
  void (*destroy)                 (DskHttpClientStreamTransfer *transfer);
};

/* note that 'funcs' must exist for the duration of the request.
 * usually this is done by having a static DskHttpClientStreamFuncs.
 * You could also free 'funcs' in the 'destroy' method.
 */
DskHttpClientStreamTransfer *
dsk_http_client_stream_request (DskHttpClientStream      *stream,
                                DskHttpRequest           *request,
				DskOctetSource           *post_data,
				DskHttpClientStreamFuncs *funcs,
				void                     *user_data);

