typedef struct _DskHttpClientStream DskHttpClientStream;
struct _DskHttpClientStream
{
  DskOctetSink *sink;
  DskOctetSource *source;
  DskHttpClientStreamTransfer *first_transfer, *last_transfer;
};

DskHttpClientStreamTransfer *
dsk_http_client_stream_request (DskHttpClientStream *stream,
                                DskHttpRequest      *request,
				DskOctetSource      *post_data,
				DskHttpRequestFunc   func,
				DskHttpRequestErrorFunc error_func,
				void                *func_data,
				DskHookDestroy       destroy);


