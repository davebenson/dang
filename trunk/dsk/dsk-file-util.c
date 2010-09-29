#include "dsk.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef _O_BINARY
# define MY_O_BINARY _O_BINARY
#elif defined(O_BINARY)
# define MY_O_BINARY  O_BINARY
#else
# define MY_O_BINARY  0
#endif

char       *dsk_file_get_contents (const char *filename,
                                   size_t     *size_out,
			           DskError  **error)
{
  DskBuffer buffer = DSK_BUFFER_STATIC_INIT;
  int fd = open (filename, O_RDONLY | MY_O_BINARY);
  int read_rv;
  if (fd < 0)
    {
      dsk_set_error (error, "error opening %s: %s",
                     filename, strerror (errno));
      return NULL;
    }
  while ((read_rv = dsk_buffer_readv (&buffer, fd)) > 0)
    ;
  if (read_rv < 0)
    {
      dsk_set_error (error,
                     "error reading file %s: %s",
                     filename, strerror (errno));
      close (fd);
      return NULL;
    }
  if (size_out)
    *size_out = buffer.size;
  char *rv;
  rv = dsk_malloc (buffer.size + 1);
  rv[buffer.size] = 0;
  dsk_buffer_read (&buffer, buffer.size, rv);
  close (fd);
  return rv;
}

dsk_boolean dsk_file_set_contents (const char *filename,
                                   size_t      size,
                                   uint8_t    *contents,
			           DskError  **error)
{
  int fd;
  unsigned rem;
retry_open:
  fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC | MY_O_BINARY, 0666);
  if (fd < 0)
    {
      if (errno == EINTR)
        goto retry_open;
      dsk_set_error (error, "error creating %s: %s",
                     filename, strerror (errno));
      return DSK_FALSE;
    }
  rem = size;
  while (rem > 0)
    {
      int write_rv = write (fd, contents, rem);
      if (write_rv < 0)
        {
          if (errno == EINTR)
            continue;
          dsk_set_error (error, "error writing to %s: %s",
                         filename, strerror (errno));
          close (fd);
          return DSK_FALSE;
        }
      rem -= write_rv;
      contents += write_rv;
    }
  close (fd);
  return DSK_TRUE;
}

dsk_boolean dsk_file_test_exists  (const char *filename)
{
  struct stat stat_buf;
retry_stat:
  if (stat (filename, &stat_buf) < 0)
    {
      if (errno == EINTR)
        goto retry_stat;
      return DSK_FALSE;
    }
  return DSK_TRUE;
}
