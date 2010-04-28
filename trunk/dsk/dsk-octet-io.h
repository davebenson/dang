
typedef struct _DskOctetSourceClass DskOctetSourceClass;
typedef struct _DskOctetSinkClass DskOctetSinkClass;
typedef struct _DskOctetSource DskOctetSource;
typedef struct _DskOctetSink DskOctetSink;

struct _DskOctetSourceClass
{
  DskObjectClass base_class;

  /* returns -1 on error */
  DskIOResult (*read)        (DskOctetSource *source,
                              unsigned        max_len,
                              void           *data_out,
                              unsigned       *bytes_read_out,
                              DskError      **error);
  DskIOResult (*read_buffer) (DskOctetSource *source,
                              DskBuffer      *read_buffer,
                              DskError      **error);

  /* not always implemented */
  void        (*shutdown)    (DskOctetSource *source);
};

struct _DskOctetSource
{
  DskObject base_instance;
  DskHook readable_hook;
};

struct _DskOctetSinkClass
{
  DskObjectClass base_class;

  /* returns -1 on error */
  DskIOResult (*write)        (DskOctetSink   *sink,
                               unsigned        max_len,
                               const void     *data_out,
                               unsigned       *n_written_out,
                               DskError      **error);
  DskIOResult (*write_buffer) (DskOctetSink   *sink,
                               DskBuffer      *write_buffer,
                               DskError      **error);

  /* not always implemented */
  void        (*shutdown)     (DskOctetSink   *sink);
};
struct _DskOctetSink
{
  DskObject base_instance;
  DskHook writable_hook;
};
#define DSK_OCTET_SOURCE(object) DSK_OBJECT_CAST(DskOctetSource, object, &dsk_octet_source_class)
#define DSK_OCTET_SINK(object) DSK_OBJECT_CAST(DskOctetSink, object, &dsk_octet_sink_class)
#define DSK_OCTET_SOURCE_GET_CLASS(object) DSK_OBJECT_CAST_GET_CLASS(DskOctetSource, object, &dsk_octet_source_class)
#define DSK_OCTET_SINK_GET_CLASS(object) DSK_OBJECT_CAST_GET_CLASS(DskOctetSink, object, &dsk_octet_sink_class)

DSK_INLINE_FUNC DskIOResult dsk_octet_source_read (void         *octet_source,
                                                   unsigned      max_len,
                                                   void         *data_out,
                                                   unsigned     *n_read_out,
                                                   DskError    **error);
DSK_INLINE_FUNC DskIOResult dsk_octet_source_read_buffer (void           *octet_source,
                                                  DskBuffer      *read_buffer,
                                                  DskError      **error);
DSK_INLINE_FUNC void dsk_octet_source_shutdown   (void           *octet_source);
DSK_INLINE_FUNC DskIOResult dsk_octet_sink_write (void           *octet_sink,
                                                  unsigned        max_len,
                                                  const void     *data,
                                                  unsigned       *n_written_out,
                                                  DskError      **error);
DSK_INLINE_FUNC DskIOResult dsk_octet_sink_write_buffer  (void           *octet_sink,
                                                  DskBuffer      *write_buffer,
                                                  DskError      **error);
DSK_INLINE_FUNC void dsk_octet_sink_shutdown     (void           *octet_sink);


typedef struct _DskOctetConnectionClass DskOctetConnectionClass;
typedef struct _DskOctetConnection DskOctetConnection;
typedef struct _DskOctetConnectionOptions DskOctetConnectionOptions;

struct _DskOctetConnectionClass
{
  DskObjectClass base_class;
};
struct _DskOctetConnection
{
  DskObject base_instance;
  DskBuffer buffer;
  DskOctetSource *source;
  DskOctetSink *sink;
  DskHookTrap *read_trap;
  DskHookTrap *write_trap;

  unsigned max_buffer;
  unsigned shutdown_on_read_error : 1;
  unsigned shutdown_on_write_error : 1;

  /* tracking the latest error */
  unsigned last_error_from_reading : 1;
  DskError *last_error;
};

struct _DskOctetConnectionOptions
{
  unsigned max_buffer;
  dsk_boolean shutdown_on_read_error;
  dsk_boolean shutdown_on_write_error;
};
#define DSK_OCTET_CONNECTION_OPTIONS_DEFAULT \
{ 4096,                         /* max_buffer */                \
  DSK_TRUE,                     /* shutdown_on_read_error */    \
  DSK_TRUE                      /* shutdown_on_write_error */   \
}

DSK_INLINE_FUNC void dsk_octet_connect       (DskOctetSource *source,
                                              DskOctetSink   *sink,
                                              DskOctetConnectionOptions *opt);

DskOctetConnection *dsk_octet_connection_new (DskOctetSource *source,
                                              DskOctetSink   *sink,
                                              DskOctetConnectionOptions *opt);
void                dsk_octet_connection_shutdown (DskOctetConnection *);
void                dsk_octet_connection_disconnect (DskOctetConnection *);

extern DskOctetSourceClass dsk_octet_source_class;
extern DskOctetSinkClass dsk_octet_sink_class;
extern DskOctetConnectionClass dsk_octet_connection_class;

#if DSK_CAN_INLINE || DSK_IMPLEMENT_INLINES
DSK_INLINE_FUNC DskIOResult dsk_octet_source_read (void         *octet_source,
                                           unsigned      max_len,
                                           void         *data_out,
                                           unsigned     *n_read_out,
                                           DskError    **error)
{
  DskOctetSourceClass *c = DSK_OCTET_SOURCE_GET_CLASS (octet_source);
  return c->read (octet_source, max_len, data_out, n_read_out, error);
}
DSK_INLINE_FUNC DskIOResult dsk_octet_source_read_buffer (void           *octet_source,
                                                  DskBuffer      *read_buffer,
                                                  DskError      **error)
{
  DskOctetSourceClass *c = DSK_OCTET_SOURCE_GET_CLASS (octet_source);
  return c->read_buffer (octet_source, read_buffer, error);
}
DSK_INLINE_FUNC void dsk_octet_source_shutdown (void           *octet_source)
{
  DskOctetSourceClass *c = DSK_OCTET_SOURCE_GET_CLASS (octet_source);
  if (c->shutdown != NULL)
    c->shutdown (octet_source);
}
DSK_INLINE_FUNC DskIOResult dsk_octet_sink_write (void           *octet_sink,
                                                  unsigned        max_len,
                                                  const void     *data,
                                                  unsigned       *n_written_out,
                                                  DskError      **error)
{
  DskOctetSinkClass *c = DSK_OCTET_SINK_GET_CLASS (octet_sink);
  return c->write (octet_sink, max_len, data, n_written_out, error);
}
DSK_INLINE_FUNC DskIOResult dsk_octet_sink_write_buffer  (void           *octet_sink,
                                                  DskBuffer      *write_buffer,
                                                  DskError      **error)
{
  DskOctetSinkClass *c = DSK_OCTET_SINK_GET_CLASS (octet_sink);
  return c->write_buffer (octet_sink, write_buffer, error);
}
DSK_INLINE_FUNC void dsk_octet_sink_shutdown     (void           *octet_sink)
{
  DskOctetSinkClass *c = DSK_OCTET_SINK_GET_CLASS (octet_sink);
  if (c->shutdown != NULL)
    c->shutdown (octet_sink);
}
DSK_INLINE_FUNC void dsk_octet_connect       (DskOctetSource *source,
                                              DskOctetSink   *sink,
                                              DskOctetConnectionOptions *opt)
{
  dsk_object_unref (dsk_octet_connection_new (source, sink, opt));
}
#endif
