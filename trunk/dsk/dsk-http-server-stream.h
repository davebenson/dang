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

  /* front of list */
  DskHttpServerStreamTransfer *first_transfer;

  /* end of list */
  DskHttpServerStreamTransfer *last_transfer;

  DskHttpServerStreamTransfer *read_transfer;

  /* to be returned by dsk_http_server_stream_get_request() */
  DskHttpServerStreamTransfer *next_request;
  DskHook request_available;
};

/* internals */
typedef enum
{
  DSK_HTTP_SERVER_STREAM_READ_NEED_HEADER,
  DSK_HTTP_SERVER_STREAM_READ_IN_POST,
  DSK_HTTP_SERVER_STREAM_READ_IN_POST_EOF,
  DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNKED_HEADER,
  DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNKED_HEADER_EXTENSION,
  DSK_HTTP_SERVER_STREAM_READ_IN_XFER_CHUNK,
  DSK_HTTP_SERVER_STREAM_READ_AFTER_XFER_CHUNK,
  DSK_HTTP_SERVER_STREAM_READ_AFTER_XFER_CHUNKED,
  DSK_HTTP_SERVER_STREAM_READ_XFER_CHUNK_TRAILER,
  DSK_HTTP_SERVER_STREAM_READ_XFER_CHUNK_FINAL_NEWLINE,
  DSK_HTTP_SERVER_STREAM_READ_DONE,
} DskHttpServerStreamReadState;
struct _DskHttpServerStreamTransfer
{
  DskHttpServerStream *owner;
  DskHttpRequest *request;
  DskHttpResponse *response;
  DskMemorySource *content;      
  DskHttpServerStreamTransfer *next;
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
dsk_http_server_stream_new     (DskOctetSink        *sink,
                                DskOctetSource      *source);
