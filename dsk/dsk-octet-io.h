
typedef struct _DskOctetSource DskOctetSource;

struct _DskOctetSource
{
  DskObject base_instance;
  DskHook *readable_hook;

  /* returns -1 on error */
  int         (*read)        (DskOctetSource *source,
                              unsigned        max_len,
                              void           *data_out,
                              DskError      **error);
  int         (*read_buffer) (DskOctetSource *source,
                              DskBuffer      *read_buffer,
                              DskError      **error);

};
typedef struct _DskOctetSink DskOctetSink;

struct _DskOctetSink
{
  DskObject base_instance;
  DskHook *writable_hook;

  /* returns -1 on error */
  int         (*write)       (DskOctetSink   *sink,
                              unsigned        max_len,
                              const void     *data_out,
                              DskError      **error);
  int         (*write_buffer) (DskOctetSink   *sink,
                               DskBuffer      *write_buffer,
                               DskError      **error);

};
