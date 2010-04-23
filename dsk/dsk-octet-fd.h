
typedef struct _DskOctetSourceStreamFdClass DskOctetSourceStreamFdClass;
typedef struct _DskOctetSourceStreamFd DskOctetSourceStreamFd;
typedef struct _DskOctetSinkStreamFdClass DskOctetSinkStreamFdClass;
typedef struct _DskOctetSinkStreamFd DskOctetSinkStreamFd;
typedef struct _DskOctetStreamFdClass DskOctetStreamFdClass;
typedef struct _DskOctetStreamFd DskOctetStreamFd;

DskOctetSource *dsk_octet_source_new_stdin (void);
DskOctetSink *dsk_octet_sink_new_stdout (void);

struct _DskOctetSourceStreamFdClass
{
  DskOctetSourceClass base_class;
};
struct _DskOctetSourceStreamFd
{
  DskOctetSource base_instance;
  DskOctetStreamFd *owner;
};

struct _DskOctetSinkStreamFdClass
{
  DskOctetSinkClass base_class;
};
struct _DskOctetSinkStreamFd
{
  DskOctetSink base_instance;
  DskOctetStreamFd *owner;
};

struct _DskOctetStreamFdClass
{
  DskObjectClass base_class;
};
struct _DskOctetStreamFd
{
  DskObject base_instance;
  DskOctetSinkStreamFd *sink;
  DskOctetSourceStreamFd *source;
};
typedef enum
{
  DSK_FILE_DESCRIPTOR_IS_READABLE              = (1<<0),
  DSK_FILE_DESCRIPTOR_IS_NOT_READABLE          = (1<<1),
  DSK_FILE_DESCRIPTOR_IS_WRITABLE              = (1<<2),
  DSK_FILE_DESCRIPTOR_IS_NOT_WRITABLE          = (1<<3),
  DSK_FILE_DESCRIPTOR_IS_POLLABLE              = (1<<4),
} DskFileDescriptorStreamFlags;

DskOctetStreamFd *dsk_octet_stream_new_fd (DskFileDescriptor fd,
                                         DskOctetSink    **in,
                                         DskOctetSource  **out,
                                         DskFileDescriptorStreamFlags flags,
                                         DskError        **error);
