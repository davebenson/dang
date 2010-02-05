
typedef struct _DskOctetSourceClass DskOctetSourceClass;
typedef struct _DskOctetSinkClass DskOctetSinkClass;
typedef struct _DskOctetSource DskOctetSource;
typedef struct _DskOctetSink DskOctetSink;

struct _DskOctetSourceClass
{
  DskObjectClass base_class;

  /* returns -1 on error */
  int         (*read)        (DskOctetSource *source,
                              unsigned        max_len,
                              void           *data_out,
                              DskError      **error);
  int         (*read_buffer) (DskOctetSource *source,
                              DskBuffer      *read_buffer,
                              DskError      **error);

  /* not always implemented */
  void        (*shutdown)    (DskOctetSource *source);
};

struct _DskOctetSource
{
  DskObject base_instance;
  DskHook *readable_hook;
};

struct _DskOctetSinkClass
{
  DskObjectClass base_class;

  /* returns -1 on error */
  int         (*write)        (DskOctetSink   *sink,
                               unsigned        max_len,
                               const void     *data_out,
                               DskError      **error);
  int         (*write_buffer) (DskOctetSink   *sink,
                               DskBuffer      *write_buffer,
                               DskError      **error);

  /* not always implemented */
  void        (*shutdown)     (DskOctetSink   *sink);
};
struct _DskOctetSink
{
  DskObject base_instance;
  DskHook *writable_hook;
};

extern DskOctetSourceClass dsk_octet_source_class;
extern DskOctetSinkClass dsk_octet_sink_class;
