#define _XOPEN_SOURCE 700

#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "dsk.h"
#include "dsk-table-helper.h"

int dsk_table_helper_openat (int openat_fd,
                             const char *base_filename,
                             const char *suffix,
                             unsigned    open_flags,
                             unsigned    open_mode,
                             DskError  **error)
{
  unsigned base_fname_len = strlen (base_filename);
  unsigned suffix_len = strlen (suffix);
  char slab[1024];
  char *buf;
  int fd;
  if (base_fname_len + suffix_len < sizeof (slab) - 1)
    buf = slab;
  else
    buf = dsk_malloc (base_fname_len + suffix_len + 1);
  memcpy (buf + 0, base_filename, base_fname_len);
  memcpy (buf + base_fname_len, suffix, suffix_len + 1);

  fd = openat (openat_fd, buf, open_flags, open_mode);
  if (fd < 0)
    {
      dsk_set_error (error, "error running openat %s%s: %s",
                     base_filename, suffix, strerror (errno));
      if (buf != slab)
        dsk_free (buf);
      return -1;
    }
  if (buf != slab)
    dsk_free (buf);
  return fd;
}

int dsk_table_helper_pread  (int fd,
                             void *buf,
                             size_t len,
                             off_t offset)
{
  return pread (fd, buf, len, offset);
}

