#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#include "dsk.h"

DskOctetSource *dsk_octet_source_new_stdin (void)
{
  static DskOctetSource *std_input = NULL;
  static DskOctetStreamFd *fdstream = NULL;
  if (fdstream == NULL)
    {
      DskError *error = NULL;
      fdstream = dsk_octet_stream_new_fd (STDIN_FILENO,
                                          DSK_FILE_DESCRIPTOR_IS_NOT_WRITABLE
                                          |DSK_FILE_DESCRIPTOR_IS_WRITABLE
                                          |DSK_FILE_DESCRIPTOR_DO_NOT_CLOSE,
                                          &error);
      if (fdstream == NULL)
        dsk_error ("standard-input not available: %s", error->message);
      std_input = (DskOctetSource *) fdstream->source;
    }
  return std_input;
}

DskOctetSink *dsk_octet_sink_new_stdout (void)
{
  static DskOctetSink *std_output = NULL;
  static DskOctetStreamFd *fdstream = NULL;
  if (fdstream == NULL)
    {
      DskError *error = NULL;
      fdstream = dsk_octet_stream_new_fd (STDOUT_FILENO,
                                          DSK_FILE_DESCRIPTOR_IS_NOT_WRITABLE
                                          |DSK_FILE_DESCRIPTOR_IS_WRITABLE
                                          |DSK_FILE_DESCRIPTOR_DO_NOT_CLOSE,
                                          &error);
      if (fdstream == NULL)
        dsk_error ("standard-input not available: %s", error->message);
      else
        std_output = (DskOctetSink *) fdstream->sink;
    }
  return std_output;
}

static void
handle_fd_ready (DskFileDescriptor   fd,
                 unsigned       events,
                 void          *callback_data)
{
  DskOctetStreamFd *stream = callback_data;
  dsk_assert (stream->fd == fd);
  if ((events & DSK_EVENT_READABLE) != 0 && stream->source != NULL)
    dsk_hook_notify (&stream->source->base_instance.readable_hook);
  if ((events & DSK_EVENT_WRITABLE) != 0 && stream->sink != NULL)
    dsk_hook_notify (&stream->sink->base_instance.writable_hook);
}

static void
stream_set_poll (DskOctetStreamFd *stream)
{
  unsigned flags = 0;
  if (stream->sink != NULL
   && dsk_hook_is_trapped (&stream->sink->base_instance.writable_hook))
    flags |= DSK_EVENT_WRITABLE;
  if (stream->source != NULL
   && dsk_hook_is_trapped (&stream->source->base_instance.readable_hook))
    flags |= DSK_EVENT_READABLE;
  dsk_main_watch_fd (stream->fd, flags, handle_fd_ready, stream);
}
static void
sink_set_poll (DskOctetSinkStreamFd *sink, dsk_boolean is_trapped)
{
  DSK_UNUSED (is_trapped);
  stream_set_poll (sink->owner);
}
static void
source_set_poll (DskOctetSourceStreamFd *source, dsk_boolean is_trapped)
{
  DSK_UNUSED (is_trapped);
  stream_set_poll (source->owner);
}

static DskHookFuncs sink_pollable_funcs =
{
  (DskHookObjectFunc) dsk_object_ref_f,
  (DskHookObjectFunc) dsk_object_unref_f,
  (DskHookSetPoll) sink_set_poll
};
static DskHookFuncs source_pollable_funcs =
{
  (DskHookObjectFunc) dsk_object_ref_f,
  (DskHookObjectFunc) dsk_object_unref_f,
  (DskHookSetPoll) source_set_poll
};

DskOctetStreamFd *
dsk_octet_stream_new_fd (DskFileDescriptor fd,
                         DskFileDescriptorStreamFlags flags,
                         DskError        **error)
{
  int getfl_flags;
  struct stat stat_buf;
  DskOctetStreamFd *rv;
  dsk_boolean is_pollable, is_readable, is_writable;
  if (fstat (fd, &stat_buf) < 0)
    {
      dsk_set_error (error, "error calling fstat on file-descriptor %u",
                     (unsigned) fd);
      return NULL;
    }
  getfl_flags = fcntl (fd, F_GETFL);
  is_pollable = S_ISFIFO (stat_buf.st_mode)
             || S_ISSOCK (stat_buf.st_mode)
             || S_ISCHR (stat_buf.st_mode)
             || isatty (fd);
  is_readable = (getfl_flags & O_ACCMODE) == O_RDONLY
             || (getfl_flags & O_ACCMODE) == O_RDWR;
  is_writable = (getfl_flags & O_ACCMODE) == O_WRONLY
             || (getfl_flags & O_ACCMODE) == O_RDWR;
  if ((flags & DSK_FILE_DESCRIPTOR_IS_NOT_READABLE) != 0)
    {
      dsk_assert ((flags & DSK_FILE_DESCRIPTOR_IS_READABLE) == 0);
#if 0                   /* maybe? */
      if (is_readable)
        shutdown (fd, SHUT_RD);
#endif
      is_readable = DSK_FALSE;
    }
  else
    {
      if ((flags & DSK_FILE_DESCRIPTOR_IS_READABLE) != 0
        && !is_readable)
        {
          dsk_set_error (error, "file-descriptor %u is not readable",
                         (unsigned) fd);
          return NULL;
        }
    }
  if ((flags & DSK_FILE_DESCRIPTOR_IS_NOT_WRITABLE) != 0)
    {
      dsk_assert ((flags & DSK_FILE_DESCRIPTOR_IS_WRITABLE) == 0);
#if 0                   /* maybe? */
      if (is_writable)
        shutdown (fd, SHUT_WR);
#endif
      is_writable = DSK_FALSE;
    }
  else
    {
      if ((flags & DSK_FILE_DESCRIPTOR_IS_WRITABLE) != 0
        && !is_writable)
        {
          dsk_set_error (error, "file-descriptor %u is not writable",
                         (unsigned) fd);
          return NULL;
        }
    }
  if (flags & DSK_FILE_DESCRIPTOR_IS_POLLABLE)
    is_pollable = DSK_TRUE;
  else if (flags & DSK_FILE_DESCRIPTOR_IS_NOT_POLLABLE)
    is_pollable = DSK_FALSE;

  rv = dsk_object_new (&dsk_octet_stream_fd_class);
  rv->fd = fd;
  if (is_pollable)
    rv->is_pollable = 1;
  if (flags & DSK_FILE_DESCRIPTOR_DO_NOT_CLOSE)
    rv->do_not_close = 1;
  if (is_readable)
    {
      rv->source = dsk_object_new (&dsk_octet_source_stream_fd_class);
      rv->source->owner = rv;
      dsk_object_ref (rv);
    }
  if (is_writable)
    {
      rv->sink = dsk_object_new (&dsk_octet_sink_stream_fd_class);
      rv->sink->owner = rv;
      dsk_object_ref (rv);
    }
  if (is_pollable)
    {
      if (rv->sink)
        dsk_hook_set_funcs (&rv->sink->base_instance.writable_hook,
                            &sink_pollable_funcs);
      if (rv->source)
        dsk_hook_set_funcs (&rv->source->base_instance.readable_hook,
                            &source_pollable_funcs);
    }
  else
    {
      if (rv->sink)
        dsk_hook_set_idle_notify (&rv->sink->base_instance.writable_hook,
                                  DSK_TRUE);
      if (rv->source)
        dsk_hook_set_idle_notify (&rv->source->base_instance.readable_hook,
                                  DSK_TRUE);
    }
  return rv;
}

static void
dsk_octet_sink_stream_fd_finalize  (DskOctetSinkStreamFd *sink)
{
  if (sink->owner)
    {
      DskOctetStreamFd *stream = sink->owner;
      sink->owner = NULL;
      stream->sink = NULL;
      dsk_object_unref (stream);
    }
}

static DskIOResult
dsk_octet_sink_stream_fd_write  (DskOctetSink   *sink,
                                 unsigned        max_len,
                                 const void     *data_out,
                                 unsigned       *n_written_out,
                                 DskError      **error)
{
  DskOctetSinkStreamFd *s = (DskOctetSinkStreamFd *) sink;
  int rv;
  if (s->owner == NULL)
    {
      dsk_set_error (error, "writing to defunct file-descriptor");
      return DSK_IO_RESULT_ERROR;
    }
  if (max_len == 0)
    {
      *n_written_out = 0;
      return DSK_IO_RESULT_SUCCESS;
    }
  rv = write (s->owner->fd, data_out, max_len);
  if (rv < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        return DSK_IO_RESULT_AGAIN;
      dsk_set_error (error, "error writing file-descriptor %u: %s",
                     s->owner->fd, strerror (errno));
      return DSK_IO_RESULT_ERROR;
    }
  *n_written_out = rv;
  return DSK_IO_RESULT_SUCCESS;
}

static DskIOResult
dsk_octet_sink_stream_fd_write_buffer (DskOctetSink   *sink,
                                       DskBuffer      *write_buffer,
                                       DskError      **error)
{
  int rv;
  DskOctetStreamFd *stream = ((DskOctetSinkStreamFd*)sink)->owner;
  if (stream == NULL)
    {
      dsk_set_error (error, "write to dead stream");
      return DSK_IO_RESULT_ERROR;
    }
  rv = dsk_buffer_writev (write_buffer, stream->fd);
  if (rv < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        return DSK_IO_RESULT_AGAIN;
      dsk_set_error (error, "error writing data to fd %u: %s",
                     stream->fd, strerror (errno));
      return DSK_IO_RESULT_ERROR;
    }
  return DSK_IO_RESULT_SUCCESS;
}


static void       
dsk_octet_sink_stream_fd_shutdown     (DskOctetSink   *sink)
{
  DskOctetStreamFd *stream = ((DskOctetSinkStreamFd*)sink)->owner;
  if (stream == NULL)
    return;
  shutdown (stream->fd, SHUT_WR);
  stream->sink = NULL;
  dsk_object_unref (stream);
  ((DskOctetSinkStreamFd*)sink)->owner = NULL;
  dsk_hook_clear (&sink->writable_hook);
}


DskOctetSinkStreamFdClass dsk_octet_sink_stream_fd_class =
{
  {
    DSK_OBJECT_CLASS_DEFINE(DskOctetSinkStreamFd,
                            &dsk_octet_sink_class,
                            NULL,
                            dsk_octet_sink_stream_fd_finalize),
    dsk_octet_sink_stream_fd_write,
    dsk_octet_sink_stream_fd_write_buffer,
    dsk_octet_sink_stream_fd_shutdown
  }
};
static void
dsk_octet_source_stream_fd_finalize  (DskOctetSourceStreamFd *source)
{
  if (source->owner)
    {
      DskOctetStreamFd *stream = source->owner;
      source->owner = NULL;
      stream->source = NULL;
      dsk_object_unref (stream);
    }
}

static DskIOResult
dsk_octet_source_stream_fd_read (DskOctetSource *source,
                               unsigned        max_len,
                               void           *data_out,
                               unsigned       *bytes_read_out,
                               DskError      **error)
{
  int n_read;
  DskOctetStreamFd *stream = ((DskOctetSourceStreamFd*)source)->owner;
  if (stream == NULL)
    {
      dsk_set_error (error, "write to dead client stream");
      return DSK_IO_RESULT_ERROR;
    }
  if (stream->fd < 0)
    {
      dsk_set_error (error, "no file-descriptor");
      return DSK_IO_RESULT_ERROR;
    }
  if (max_len == 0)
    {
      *bytes_read_out = 0;
      return DSK_IO_RESULT_SUCCESS;
    }
  n_read = read (stream->fd, data_out, max_len);
  if (n_read < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        return DSK_IO_RESULT_AGAIN;
      dsk_set_error (error, "error reading from client stream (fd %d): %s",
                     stream->fd, strerror (errno));
      return DSK_IO_RESULT_ERROR;
    }
  if (n_read == 0)
    return DSK_IO_RESULT_EOF;
  *bytes_read_out = n_read;
  return DSK_IO_RESULT_SUCCESS;
}

static DskIOResult
dsk_octet_source_stream_fd_read_buffer  (DskOctetSource *source,
                                       DskBuffer      *read_buffer,
                                       DskError      **error)
{
  int rv;
  DskOctetStreamFd *stream = ((DskOctetSourceStreamFd*)source)->owner;
  if (stream == NULL)
    {
      dsk_set_error (error, "read from dead stream");
      return DSK_IO_RESULT_ERROR;
    }
  if (stream->fd < 0)
    {
      dsk_set_error (error, "read from stream with no file-descriptor");
      return DSK_IO_RESULT_ERROR;
    }
  rv = dsk_buffer_readv (read_buffer, stream->fd);
  if (rv < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        return DSK_IO_RESULT_AGAIN;
      dsk_set_error (error, "error reading data from fd %u: %s",
                     stream->fd, strerror (errno));
      return DSK_IO_RESULT_ERROR;
    }
  if (rv == 0)
    return DSK_IO_RESULT_EOF;
  return DSK_IO_RESULT_SUCCESS;
}

static void
dsk_octet_source_stream_fd_shutdown (DskOctetSource *source)
{
  DskOctetStreamFd *stream = ((DskOctetSourceStreamFd*)source)->owner;
  if (stream == NULL)
    return;
  shutdown (stream->fd, SHUT_RD);

  stream->source = NULL;
  dsk_object_unref (stream);
  ((DskOctetSourceStreamFd*)source)->owner = NULL;

  dsk_hook_clear (&source->readable_hook);
}


DskOctetSourceStreamFdClass dsk_octet_source_stream_fd_class =
{
  { DSK_OBJECT_CLASS_DEFINE(DskOctetSourceStreamFd,
                            &dsk_octet_source_class,
                            NULL,
                            dsk_octet_source_stream_fd_finalize),
    dsk_octet_source_stream_fd_read,
    dsk_octet_source_stream_fd_read_buffer,
    dsk_octet_source_stream_fd_shutdown
  }
};

static void
dsk_octet_stream_fd_init (DskOctetStreamFd *stream)
{
  stream->fd = -1;
}

static void
dsk_octet_stream_fd_finalize (DskOctetStreamFd *stream)
{
  dsk_assert (stream->source == NULL);
  dsk_assert (stream->sink == NULL);
  if (stream->fd >= 0 && !stream->do_not_close)
    {
      stream->fd = -1;
      close (stream->fd);
    }
}

DskOctetStreamFdClass dsk_octet_stream_fd_class =
{
  DSK_OBJECT_CLASS_DEFINE(DskOctetStreamFd,
                          &dsk_object_class,
                          dsk_octet_stream_fd_init,
                          dsk_octet_stream_fd_finalize)
};

