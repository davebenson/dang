/* TODO: dsk_octet_stream_new_fd() must somehow return refs to
 * the sink/source (optionally, at least)  but since the DskOctetStreamFd
 * does not hold refs to the sink/source, it isn't an adequate of handling this
 */

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
  DskFileDescriptor fd;
  unsigned do_not_close : 1;
  unsigned is_pollable : 1;
};
typedef enum
{
  DSK_FILE_DESCRIPTOR_IS_READABLE              = (1<<0),
  DSK_FILE_DESCRIPTOR_IS_NOT_READABLE          = (1<<1),
  DSK_FILE_DESCRIPTOR_IS_WRITABLE              = (1<<2),
  DSK_FILE_DESCRIPTOR_IS_NOT_WRITABLE          = (1<<3),
  DSK_FILE_DESCRIPTOR_IS_POLLABLE              = (1<<4),
  DSK_FILE_DESCRIPTOR_IS_NOT_POLLABLE          = (1<<5),
  DSK_FILE_DESCRIPTOR_DO_NOT_CLOSE             = (1<<16)
} DskFileDescriptorStreamFlags;

DskOctetStreamFd *dsk_octet_stream_new_fd (DskFileDescriptor fd,
                                         DskFileDescriptorStreamFlags flags,
                                         DskError        **error);

extern DskOctetSinkStreamFdClass dsk_octet_sink_stream_fd_class;
extern DskOctetSourceStreamFdClass dsk_octet_source_stream_fd_class;
extern DskOctetStreamFdClass dsk_octet_stream_fd_class;
