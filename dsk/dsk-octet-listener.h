
typedef struct _DskOctetListenerClass DskOctetListenerClass;
typedef struct _DskOctetListener DskOctetListener;

struct _DskOctetListenerClass
{
  DskObjectClass base_class;
  DskIOResult (*accept) (DskOctetListener        *listener,
                         DskOctetStream         **stream_out,
			 DskOctetSource         **source_out,
			 DskOctetSink           **sink_out,
                         DskError               **error);
};

struct _DskOctetListener
{
  DskObject base_instance;
  DskHook   incoming;
};

DskIOResult dsk_octet_listener_accept (DskOctetListener        *listener,
                                       DskOctetStream         **stream_out,
			               DskOctetSource         **source_out,
			               DskOctetSink           **sink_out,
                                       DskError               **error);

extern const DskOctetListenerClass dsk_octet_listener_class;
