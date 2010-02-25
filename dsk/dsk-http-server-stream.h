typedef struct _DskHttpServerStreamClass DskHttpServerStreamClass;
typedef struct _DskHttpServerStream DskHttpServerStream;
typedef struct _DskHttpServerStreamFuncs DskHttpServerStreamFuncs;
typedef struct _DskHttpServerStreamTransfer DskHttpServerStreamTransfer;

struct _DskHttpServerStreamClass
{
  DskObject base_instance;
};
struct _DskHttpServerStream
{
  DskObject base_instance;
  DskOctetSink *sink;
  DskOctetSource *source;
  DskBuffer incoming_data;
  DskBuffer outcoming_data;
  DskHttpServerStreamTransfer *first_transfer, *last_transfer;
  DskHttpServerStreamTransfer *incoming_data_transfer, *outgoing_data_transfer;
};

/* internals */
typedef enum
{
  DSK_HTTP_CLIENT_STREAM_READ_NEED_HEADER,
  DSK_HTTP_CLIENT_STREAM_READ_IN_BODY,
  DSK_HTTP_CLIENT_STREAM_READ_IN_BODY_EOF,
  DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNKED_HEADER,
  DSK_HTTP_CLIENT_STREAM_READ_IN_XFER_CHUNK,
  DSK_HTTP_CLIENT_STREAM_READ_DONE
} DskHttpServerStreamReadState;
struct _DskHttpServerStreamTransfer
{
  DskHttpServerStream *owner;
  DskHttpRequest *request;
  DskHttpResponse *response;
  DskMemoryBufferSource *content;      
  DskHttpServerStreamTransfer *next;
  DskHttpServerStreamFuncs *funcs;
  void *user_data;
  DskHttpServerStreamReadState read_state;
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

DskHttpServerStream *
dsk_http_client_stream_new     (DskOctetSink        *sink,
                                DskOctetSource      *source);

typedef enum
{
  DSK_HTTP_CLIENT_STREAM_ERROR_PREMATURE_SHUTDOWN,
  DSK_HTTP_CLIENT_STREAM_ERROR_BAD_HEADER,
  DSK_HTTP_CLIENT_STREAM_ERROR_BAD_CONTENT,
} DskHttpServerStreamError;

struct _DskHttpServerStreamFuncs
{
  /* called once the http header is received.
     content may or may not be complete. */
  void (*handle_request)         (DskHttpServerStreamTransfer *transfer);

  /* called once (posted) content is completely received */
  void (*handle_content_complete) (DskHttpServerStreamTransfer *transfer);

  /* call for any number of errors */
  void (*handle_error)            (DskHttpServerStreamTransfer *transfer);

  /* always called exactly once after all other functions done */
  void (*destroy)                 (DskHttpServerStreamTransfer *transfer);
};

/* note that 'funcs' must exist for the duration of the request.
 * usually this is done by having a static DskHttpServerStreamFuncs.
 * You could also free 'funcs' in the 'destroy' method.
 */
DskHttpServerStreamTransfer *
dsk_http_client_stream_request (DskHttpServerStream      *stream,
                                DskHttpRequest           *request,
				DskOctetSource           *post_data,
				DskHttpServerStreamFuncs *funcs,
				void                     *user_data);

