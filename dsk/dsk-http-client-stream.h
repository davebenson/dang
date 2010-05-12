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
  DskHookTrap *write_trap;
  DskOctetSource *source;
  DskHookTrap *read_trap;
  DskBuffer incoming_data;
  DskBuffer outcoming_data;
  DskHttpClientStreamTransfer *first_transfer, *last_transfer;
  DskHttpClientStreamTransfer *incoming_data_transfer, *outgoing_data_transfer;

  DskError *latest_error;
  DskHook error_hook;
};

/* internals */
typedef enum
{
  DSK_HTTP_CLIENT_STREAM_READ_INIT,
  DSK_HTTP_CLIENT_STREAM_READ_NEED_HEADER,
  DSK_HTTP_CLIENT_STREAM_READ_IN_BODY,
  DSK_HTTP_CLIENT_STREAM_READ_IN_BODY_EOF,
  DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNKED_HEADER,
  DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNK,
  DSK_HTTP_CLIENT_STREAM_READ_DONE
} DskHttpClientStreamReadState;
struct _DskHttpClientStreamTransfer
{
  DskHttpClientStream *owner;
  DskHttpRequest *request;
  DskHttpResponse *response;
  DskMemorySource *content;      
  DskHttpClientStreamTransfer *next;
  DskHttpClientStreamFuncs *funcs;
  void *user_data;
  DskHttpClientStreamReadState read_state;
  /* branch of union depends on 'read_state' */
  union {
    /* number of bytes we've already checked for end of header. */
    struct { unsigned checked; } need_header;

    /* number of bytes remaining in content-length */
    struct { uint64_t remaining; } in_body;

    /* number of bytes remaining in current chunk */
    struct { uint64_t remaining; } in_xfer_chunk;

    /* no data for IN_XFER_CHUNKED_HEADER, DONE */
  } read_info;
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

