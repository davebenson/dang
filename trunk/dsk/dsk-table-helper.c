#define _ATFILE_SOURCE
#define _XOPEN_SOURCE 700
#define _ATFILE_SOURCE

#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "dsk.h"
#include "dsk-table-helper.h"

int dsk_table_helper_openat (const char *openat_dir,
                             int         openat_fd,
                             const char *base_filename,
                             const char *suffix,
                             unsigned    open_flags,
                             unsigned    open_mode,
                             DskError  **error)
{
  unsigned base_fname_len = strlen (base_filename);
  unsigned suffix_len = strlen (suffix);
  unsigned openat_dir_len = strlen (openat_dir);
  char slab[1024];
  char *buf;
  int fd;
#if !defined(__USE_ATFILE)
  DSK_UNUSED (openat_fd);
  base_fname_len += openat_dir_len + 1;
#endif
if (base_fname_len + suffix_len < sizeof (slab) - 1)
    buf = slab;
  else
    buf = dsk_malloc (base_fname_len + suffix_len + 1);
#if defined(__USE_ATFILE)
  memcpy (buf + 0, base_filename, base_fname_len);
#else
  memcpy (buf + 0, openat_dir, openat_dir_len);
  buf[openat_dir_len] = '/';
  strcpy (buf + openat_dir_len + 1, base_filename);
#endif
  memcpy (buf + base_fname_len, suffix, suffix_len + 1);

#if defined(__USE_ATFILE)
  fd = openat (openat_fd, buf, open_flags, open_mode);
#define OPEN_SYSTEM_CALL "openat"
#else
  fd = open (buf, open_flags, open_mode);
#define OPEN_SYSTEM_CALL "open"
#endif
  if (fd < 0)
    {
      dsk_set_error (error, "error running %s %s: %s",
                     OPEN_SYSTEM_CALL, buf, strerror (errno));
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

