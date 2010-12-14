/* TODO: saw close() return EBADFD in writer/reader pairing somewhere */

/* portability issues:
     openat()
     O_NOATIME
     pread
 */

/* IDEAS:
    - mmap()
 */

/* For openat() */
#define _ATFILE_SOURCE
#define _XOPEN_SOURCE 1000

/* O_NOATIME */
#define _GNU_SOURCE


#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include "dsk.h"
#include <zlib.h>

#define INDEX_WRITER_HEAP_BUF_SIZE          4096

/* Must be a multiple of 6, since index0 has records of 6 uint32;
   all other index levels have records of 3 uint32. */
#define INDEX_WRITER_INDEX_BUF_COUNT        (6*128)

/* --- Writer --- */
typedef struct _IndexWriter IndexWriter;
struct _IndexWriter
{
  int index_fd, heap_fd;
  uint8_t heap_buf[INDEX_WRITER_HEAP_BUF_SIZE];
  unsigned heap_buf_used;
  uint64_t heap_buf_written;
  uint32_t index_buf[INDEX_WRITER_INDEX_BUF_COUNT];
  unsigned index_buf_used;
  unsigned entries_left;
  IndexWriter *up;
};

#define COMPRESSOR_BUF_SIZE 4096
struct _DskTableFileWriter
{
  z_stream compressor;
  uint8_t out_buf[COMPRESSOR_BUF_SIZE];

  /* is this duplicated work that compressor already does?
     probably not b/c of the 'reset' calls. */
  uint64_t out_buf_file_offset;

  unsigned fanout;
  unsigned entries_until_flush;
  dsk_boolean is_first;
  dsk_boolean is_closed;
  int compressed_data_fd;
  uint8_t gzip_level;
  uint64_t last_compressor_flush_offset;
  uint32_t last_gzip_size;

  unsigned first_key_len;
  uint8_t *first_key_data;

  IndexWriter *index_levels;

  uint64_t total_n_entries;
  uint64_t total_key_bytes;
  uint64_t total_value_bytes;

  /* basefilename has 20 characters of padding to allow
     adding suffixes with snprintf(basefilename+basefilename_len, 20, ...); */
  char *basefilename;
  unsigned basefilename_len;   
  int openat_fd;
};

static int
create_fd (DskTableFileWriter *writer,
           const char         *suffix,
           DskError          **error)
{
  strcpy (writer->basefilename + writer->basefilename_len, suffix);
  int fd = openat (writer->openat_fd, writer->basefilename,
                   O_CREAT|O_WRONLY|O_NOATIME|O_TRUNC, 0666);
  if (fd < 0)
    {
      dsk_set_error (error, "error opening %s: %s", writer->basefilename,
                     strerror (errno));
      return -1;
    }
  return fd;
}

static dsk_boolean
write_to_fd(int         fd, 
            size_t      size,
            const void *buf,
            DskError  **error)
{
  while (size > 0)
    {
      int write_rv = write (fd, buf, size);
      if (write_rv < 0)
        {
          if (errno == EINTR || errno == EAGAIN)
            continue;
          dsk_set_error (error, "error writing to file-descriptor %d: %s",
                         fd, strerror (errno));
          return DSK_FALSE;
        }
      size -= write_rv;
      buf = ((char*)buf) + write_rv;
    }
  return DSK_TRUE;
}

static dsk_boolean
read_from_fd(int         fd, 
             size_t      size,
             void       *buf,
             DskError  **error)
{
  while (size > 0)
    {
      int read_rv = read (fd, buf, size);
      if (read_rv < 0)
        {
          if (errno == EINTR || errno == EAGAIN)
            continue;
          dsk_set_error (error, "error reading from file-descriptor %d: %s",
                         fd, strerror (errno));
          return DSK_FALSE;
        }
      size -= read_rv;
      buf = ((char*)buf) + read_rv;
    }
  return DSK_TRUE;
}




static dsk_boolean
alloc_index_level (DskTableFileWriter *writer,
                   IndexWriter       **out,
                   unsigned            index_level,
                   DskError          **error)
{
  int index_fd, heap_fd;
  char buf[10];
  //dsk_warning ("alloc_index_level: %u", index_level);
  snprintf (buf, sizeof (buf), ".%03uh", index_level);
  heap_fd = create_fd (writer, buf, error);
  if (heap_fd < 0)
    return DSK_FALSE;
  buf[4] = 'i';
  index_fd = create_fd (writer, buf, error);
  if (index_fd < 0)
    {
      close (heap_fd);
      return DSK_FALSE;
    }
  *out = dsk_malloc (sizeof (IndexWriter));
  (*out)->index_fd = index_fd;
  (*out)->heap_fd = heap_fd;
  (*out)->heap_buf_used = 0;
  (*out)->heap_buf_written = 0;
  (*out)->index_buf_used = 0;
  (*out)->entries_left = writer->fanout;
  (*out)->up = NULL;
  return DSK_TRUE;
}
static void
free_index_level (IndexWriter *index)
{
  close (index->index_fd);
  close (index->heap_fd);
  dsk_free (index);
}

DskTableFileWriter *dsk_table_file_writer_new (DskTableFileOptions *options,
                                               DskError           **error)
{
  DskTableFileWriter *writer = dsk_malloc0 (sizeof (DskTableFileWriter));
  writer->basefilename_len = strlen (options->base_filename);
  writer->basefilename = dsk_malloc (writer->basefilename_len + 30);
  strcpy (writer->basefilename, options->base_filename);
  writer->entries_until_flush = 0;
  writer->is_first = DSK_TRUE;
  writer->fanout = options->index_fanout;
  writer->openat_fd = options->openat_fd;
  if (writer->openat_fd < 0)
    writer->openat_fd = AT_FDCWD;
  if (!alloc_index_level (writer, &writer->index_levels, 0, error))
    goto error_cleanup_0;

  /* special handling of the very first entry written requires this */
  writer->index_levels->entries_left += 1;

  writer->gzip_level = options->gzip_level;
  writer->compressed_data_fd = create_fd (writer, "", error);
  if (writer->compressed_data_fd < 0)
    goto error_cleanup_2;
  writer->compressor.avail_in = 0;
  writer->compressor.next_in = NULL;
  writer->compressor.avail_out = COMPRESSOR_BUF_SIZE;
  writer->compressor.next_out = writer->out_buf;
  int zrv;
  zrv = deflateInit2 (&writer->compressor,
                      writer->gzip_level,
                      Z_DEFLATED,
                      15, 8,
                      Z_DEFAULT_STRATEGY);
  if (zrv != Z_OK)
    {
      goto error_cleanup_3;
    }

  return writer;

error_cleanup_3:
  close (writer->compressed_data_fd);
error_cleanup_2:
  free_index_level (writer->index_levels);
error_cleanup_0:
  dsk_free (writer->index_levels);
  dsk_free (writer->basefilename);
  dsk_free (writer);
  return NULL;
}

#if DSK_IS_LITTLE_ENDIAN
# define UINT32_TO_LITTLE_ENDIAN(value)   (value)
#else
# define UINT32_TO_LITTLE_ENDIAN(value) (  (((value)&0xff) << 24) \
                                         | (((value)&0xff00) << 8) \
                                         | (((value)&0xff0000) >> 8) \
                                         | (((value)&0xff000000) >> 24) )
#endif

static dsk_boolean
flush_index_index_to_disk (IndexWriter *index,
                           DskError   **error)
{
  if (index->index_buf_used == 0)
    return DSK_TRUE;
  if (!write_to_fd (index->index_fd, index->index_buf_used * 4,
                   index->index_buf, error))
    return DSK_FALSE;
  index->index_buf_used = 0;
  return DSK_TRUE;
}

static dsk_boolean
write_index0_compressed_len (DskTableFileWriter *writer,
                             DskError          **error)
{
  IndexWriter *index = writer->index_levels;
  uint32_t *at = index->index_buf + index->index_buf_used;
  *at++ = writer->last_gzip_size;
  index->index_buf_used++;
  if (index->index_buf_used == INDEX_WRITER_INDEX_BUF_COUNT)
    {
      /* flush to disk */
      if (!flush_index_index_to_disk (index, error))
        return DSK_FALSE;
    }
  return DSK_TRUE;
}

static dsk_boolean
flush_index_heap_to_disk (IndexWriter *index,
                          DskError          **error)
{
  if (!write_to_fd (index->heap_fd, index->heap_buf_used,
                    index->heap_buf, error))
    return DSK_FALSE;
  index->heap_buf_written += index->heap_buf_used;
  index->heap_buf_used = 0;
  return DSK_TRUE;
}

static dsk_boolean
write_index_key (IndexWriter        *index,
                 unsigned            key_len,
                 const uint8_t      *key_data,
                 DskError          **error)
{
  while (index->heap_buf_used + key_len >= INDEX_WRITER_HEAP_BUF_SIZE)
    {
      unsigned used = INDEX_WRITER_HEAP_BUF_SIZE - index->heap_buf_used;
      memcpy (index->heap_buf + index->heap_buf_used, key_data, used);
      index->heap_buf_used += used;
      if (!flush_index_heap_to_disk (index, error))
        return DSK_FALSE;
      key_data += used;
      key_len -= used;
    }
  memcpy (index->heap_buf + index->heap_buf_used, key_data, key_len);
  index->heap_buf_used += key_len;
  return DSK_TRUE;
}

/* This is the first part of the index entry; the last part is
   just the compressed size of the next was of data -- but
   we haven't compressed it yet... */
static dsk_boolean
write_index0_partial_entry (DskTableFileWriter *writer,
                            unsigned            key_length,
                            const uint8_t      *key_data,
                            DskError          **error)
{
  /* Each entry is an offset into the data file,
     plus an index into the gzipped file.  (both uint64le).
     and the size of the gzipped data, and the size of the key.
     (both uint32le) */
  IndexWriter *index = writer->index_levels;
  uint64_t data_offset = index->heap_buf_written + index->heap_buf_used;
  uint64_t gzip_offset = writer->last_compressor_flush_offset;
  uint32_t *at = index->index_buf + index->index_buf_used;
  *at++ = data_offset;
  *at++ = ((data_offset) >> 32);
  *at++ = gzip_offset;
  *at++ = ((gzip_offset) >> 32);
  *at++ = key_length;
  index->index_buf_used += 5;
  return write_index_key (index, key_length, key_data, error);
}
static dsk_boolean
write_index_entry  (IndexWriter        *index,
                    unsigned            key_length,
                    const uint8_t      *key_data,
                    DskError          **error)
{
  /* Data just include offset into this index's level of heap file;
     the start and end of the range in the next finer grained index
     can be found be multiplying by fanout, and the size of the key. */
  uint64_t data_offset = index->heap_buf_written + index->heap_buf_used;
  uint32_t *at = index->index_buf + index->index_buf_used;
  *at++ = data_offset;
  *at++ = ((data_offset) >> 32);
  *at++ = key_length;
  index->index_buf_used += 3;
  if (index->index_buf_used == INDEX_WRITER_INDEX_BUF_COUNT)
    {
      if (!flush_index_index_to_disk (index, error))
        return DSK_FALSE;
    }
  return write_index_key (index, key_length, key_data, error);
}

static dsk_boolean
flush_compressor_to_disk (DskTableFileWriter *writer,
                          DskError          **error)
{
  unsigned to_write = COMPRESSOR_BUF_SIZE - writer->compressor.avail_out;
  if (to_write == 0)
    return DSK_TRUE;
  if (!write_to_fd (writer->compressed_data_fd,
                   to_write, writer->out_buf, error))
    return DSK_FALSE;
  writer->out_buf_file_offset += to_write;
  writer->compressor.next_out = writer->out_buf;
  writer->compressor.avail_out = COMPRESSOR_BUF_SIZE;
  return DSK_TRUE;
}

static dsk_boolean
flush_compressed_data (DskTableFileWriter *writer,
                       DskError          **error)
{
  writer->compressor.avail_in = 0;
  writer->compressor.next_in = NULL;
  int zrv;
  if (writer->compressor.avail_out < 6)
    if (!flush_compressor_to_disk (writer, error))
      return DSK_FALSE;
retry_deflate:
  zrv = deflate (&writer->compressor, Z_FULL_FLUSH);
  if (zrv == Z_OK && writer->compressor.avail_out > 0)
    {
      uint64_t offset = writer->out_buf_file_offset
                      + (writer->compressor.next_out - writer->out_buf);
      writer->last_gzip_size = offset - writer->last_compressor_flush_offset;
      writer->last_compressor_flush_offset = offset;
      return DSK_TRUE;
    }
  if (zrv == Z_OK)
    {
      /* Probably never happens. */
      if (!flush_compressor_to_disk (writer, error))
        return DSK_FALSE;
      goto retry_deflate;
    }
  dsk_set_error (error, "error compressing (zlib error code %d)", zrv);
  return DSK_FALSE;
}

static dsk_boolean
table_file_write_data (DskTableFileWriter *writer,
                       unsigned            length,
                       const uint8_t      *data,
                       DskError          **error)
{
  writer->compressor.next_in = (void*) data;
  writer->compressor.avail_in = length;
  while (writer->compressor.avail_in > 0)
    {
      int zrv = deflate (&writer->compressor, Z_NO_FLUSH);
      if (zrv != Z_OK)
        {
          dsk_set_error (error, "zlib's deflate returned error code %d", zrv);
          return DSK_FALSE;
        }
      if (writer->compressor.avail_out == 0)
        {
          if (!flush_compressor_to_disk (writer, error))
            return DSK_FALSE;
        }
    }
  return DSK_TRUE;
}


dsk_boolean dsk_table_file_write (DskTableFileWriter *writer,
                                  unsigned            key_length,
			          const uint8_t      *key_data,
                                  unsigned            value_length,
			          const uint8_t      *value_data,
			          DskError          **error)
{
  dsk_assert (!writer->is_closed);

  //dsk_warning ("dsk_table_file_write [0x%llx]: key_len/value_len=%u/%u",writer->total_n_entries,key_length,value_length);

  if (writer->entries_until_flush == 0)
    {
      if (writer->is_first)
        {
          writer->first_key_data = dsk_memdup (key_length, key_data);
          writer->first_key_len = key_length;
          writer->is_first = DSK_FALSE;
        }
      else
        {
          /* Flush the existing inflator */
          if (!flush_compressed_data (writer, error))
            return DSK_FALSE;
          if (!write_index0_compressed_len (writer, error))
            return DSK_FALSE;
        }

      /* Write first-level index entry */
      //dsk_warning ("writing index 0 entry");
      if (!write_index0_partial_entry (writer, key_length, key_data, error))
        return DSK_FALSE;

      /* Do we need to go the next level? */
      IndexWriter *index;
      dsk_boolean write_index = DSK_FALSE;
      if (--(writer->index_levels->entries_left) > 0)
        index = NULL;
      else
        {
          writer->index_levels->entries_left = writer->fanout;
          index = writer->index_levels->up;
          write_index = DSK_TRUE;
        }

      if (write_index)
        {
          while (index != NULL)
            {
              //dsk_warning ("writing index non-0 entry (entries_left=%u)", index->entries_left);
              if (!write_index_entry (index, key_length, key_data, error))
                return DSK_FALSE;

              /* Do we need to go the next level? */
              if (--(index->entries_left) > 0)
                break;
              index->entries_left = writer->fanout;
              index = index->up;
            }
          if (index == NULL)
            {
              /* Create a new level of index */
              unsigned new_level = 1;
              index = writer->index_levels;
              while (index->up)
                {
                  index = index->up;
                  new_level++;
                }
              if (!alloc_index_level (writer, &index->up, new_level, error))
                return DSK_FALSE;

              /* make 'index' point to the new index level */
              index = index->up;

              //dsk_warning ("writing init index %u entries", new_level);
              /* Every index should begin with the first key. */
              if (!write_index_entry (index,
                                      writer->first_key_len,
                                      writer->first_key_data,
                                      error))
                return DSK_FALSE;

              /* Write index entry */
              if (!write_index_entry (index, key_length, key_data, error))
                return DSK_FALSE;

              /* it might seem like "-= 2" would be better
                 here.  but it doesn't work.  TODO: better doc/describe
                 the role of entries_left. */
              index->entries_left -= 1;
            }
        }
      writer->entries_until_flush = writer->fanout - 1;
    }
  else
    writer->entries_until_flush -= 1;

  uint32_t littleendian_vals[2] = {
    UINT32_TO_LITTLE_ENDIAN (key_length),
    UINT32_TO_LITTLE_ENDIAN (value_length)
  };
  if (!table_file_write_data (writer, 8, (const uint8_t *) &littleendian_vals, error)
   || !table_file_write_data (writer, key_length, key_data, error)
   || !table_file_write_data (writer, value_length, value_data, error))
    return DSK_FALSE;

  writer->total_n_entries += 1;
  writer->total_key_bytes += key_length;
  writer->total_value_bytes += value_length;

  return DSK_TRUE;
}

dsk_boolean dsk_table_file_writer_close   (DskTableFileWriter *writer,
                                           DskError           **error)
{

  if (!flush_compressed_data (writer, error))
    return DSK_FALSE;
  if (!writer->is_first && !write_index0_compressed_len (writer, error))
    return DSK_FALSE;
  if (!flush_compressor_to_disk (writer, error))
    return DSK_FALSE;

  /* write metadata: number of index levels, fanout, fanout0. */
  int fd = create_fd (writer, ".info", error);
  if (fd < 0)
    return DSK_FALSE;
  char metadata_buf[1024];
  unsigned n_levels = 0;
  IndexWriter *index;
  for (index = writer->index_levels; index; index = index->up)
    {
      if (!flush_index_heap_to_disk (index, error)
       || !flush_index_index_to_disk (index, error))
        return DSK_FALSE;
      n_levels++;
    }
  snprintf (metadata_buf, sizeof (metadata_buf),
            "entries: %llu\n"
            "key-bytes: %llu\n"
            "data-bytes: %llu\n"
            "compressed-size: %llu\n"
            "n-index-levels: %u\n"
            "fanout: %u\n",
            writer->total_n_entries,
            writer->total_key_bytes,
            writer->total_value_bytes,
            writer->out_buf_file_offset,
            n_levels,
            writer->fanout);
  if (!write_to_fd (fd, strlen (metadata_buf),
                   (const uint8_t *) metadata_buf, error))
    {
      close (fd);
      return DSK_FALSE;
    }
  close (fd);

  writer->is_closed = DSK_TRUE;
  return DSK_TRUE;
}

static void
unlink_by_suffix (DskTableFileWriter *writer,
                  const char *suffix)
{
  strcpy (writer->basefilename + writer->basefilename_len, suffix);
  unlinkat (writer->openat_fd, writer->basefilename, 0);
}

void        dsk_table_file_writer_destroy (DskTableFileWriter *writer)
{
  /* Close all file descriptors */
  unsigned n_levels = 0;
  IndexWriter *index;
  for (index = writer->index_levels; index; )
    {
      IndexWriter *up = index->up;
      free_index_level (index);
      index = up;
      n_levels++;
    }
  close (writer->compressed_data_fd);

  deflateEnd (&writer->compressor);

  if (!writer->is_closed)
    {
      unsigned i;
      /* Delete all files if not closed. */
      unlink_by_suffix (writer, ".metadata");
      unlink_by_suffix (writer, "");
      for (i = 0; i < n_levels; i++)
        {
          char buf[10];
          snprintf (buf, sizeof (buf), ".%03uh", i);
          unlink_by_suffix (writer, buf);
          buf[4] = 'i';
          unlink_by_suffix (writer, buf);
        }
    }
  dsk_free (writer->basefilename);
  dsk_free (writer);
}

/* --- Reader --- */
#define READER_INBUF_SIZE  4096
#define READER_OUTBUF_SIZE 8192
typedef struct _Reader Reader;
struct _Reader
{
  DskTableFileReader base;
  z_stream decompressor;
  uint8_t inbuf[READER_INBUF_SIZE];    /* input from file to decompressor */
  uint8_t outbuf[READER_OUTBUF_SIZE];  /* output of decompressor */
  unsigned outbuf_bytes_used;    /* as in, have been returned as entries */
  int fd;
  uint8_t *scratch;
  unsigned scratch_alloced;
};

DskTableFileReader *
dsk_table_file_reader_new (DskTableFileOptions *options,
                           DskError           **error)
{
  Reader *rv;
  int fd;
  if (options->openat_fd < 0)
    fd = open (options->base_filename, O_RDONLY);
  else
    fd = openat (options->openat_fd, options->base_filename, O_RDONLY);
  if (fd < 0)
    {
      dsk_set_error (error, "%s(): error reading %s: %s",
                     options->openat_fd < 0 ? "open" : "openat", 
                     options->base_filename,
                     strerror (errno));
      return NULL;
    }
  rv = dsk_malloc (sizeof (Reader));
  memset (&rv->decompressor, 0, sizeof (z_stream));
  if (inflateInit2 (&rv->decompressor, 15) != 0)
    dsk_die ("inflateInit2 failed");
  rv->fd = fd;
  rv->base.at_eof = DSK_FALSE;
  rv->decompressor.next_out = rv->outbuf;
  rv->decompressor.avail_out = READER_OUTBUF_SIZE;
  
  DskError *e = NULL;
  if (!dsk_table_file_reader_advance (&rv->base, &e))
    {
      if (e != NULL)
        {
          dsk_table_file_reader_destroy (&rv->base);
          if (error)
            *error = e;
          else
            dsk_error_unref (e);
          return NULL;
        }
    }
  return &rv->base;
}

dsk_boolean
dsk_table_file_reader_advance  (DskTableFileReader *reader,
                                DskError          **error)
{
  Reader *r = (Reader *) reader;
  const uint8_t *o = r->outbuf + r->outbuf_bytes_used;
  unsigned decompressed = r->decompressor.next_out - o;
  int zrv;
  if (decompressed < 8)
    {
      memmove (r->outbuf, o, decompressed);
      r->decompressor.next_out = r->outbuf + decompressed;
      r->decompressor.avail_out = READER_OUTBUF_SIZE - decompressed;
      r->outbuf_bytes_used = 0;
      o = r->outbuf;
      if (r->decompressor.avail_in > 0)
        {
          zrv = inflate(&r->decompressor, r->fd < 0 ? Z_FINISH : 0);
          if (zrv != Z_OK && zrv != Z_STREAM_END && zrv != Z_BUF_ERROR)
            goto zlib_error;
          decompressed = r->decompressor.next_out - o;
        }
      if (decompressed < 8)
        {
          if (r->fd < 0)
            {
              /* at EOF. */
              if (decompressed > 0)
                dsk_set_error (error, "partial data at end of buffer");
              r->base.at_eof = DSK_TRUE;
              return DSK_FALSE;
            }

          /* read more data */
          if (r->decompressor.avail_in > 0)
            memmove (r->inbuf, r->decompressor.next_in,
                     r->decompressor.avail_in);
          int read_rv;
retry_read:
          read_rv = read (r->fd,
                          r->inbuf + r->decompressor.avail_in,
                          READER_INBUF_SIZE - r->decompressor.avail_in);
          if (read_rv == 0)
            {
              close (r->fd);
              r->fd = -1;
            }
          else if (read_rv < 0)
            {
              if (errno == EINTR || errno == EAGAIN)
                goto retry_read;
              dsk_set_error (error, "error reading from file: %s",
                             strerror (errno));
              return DSK_FALSE;
            }
          else
            {
              r->decompressor.avail_in += read_rv;
              r->decompressor.next_in = r->inbuf;
            }
          if (r->decompressor.avail_in > 0)
            {
              zrv = inflate (&r->decompressor, r->fd < 0 ? Z_FINISH : 0);
              decompressed = r->decompressor.next_out - r->outbuf;
              if (zrv != Z_OK && zrv != Z_STREAM_END && zrv != Z_BUF_ERROR)
                goto zlib_error;
            }
        }
      if (decompressed == 0)
        {
          r->base.at_eof = DSK_TRUE;
          return DSK_FALSE;
        }
      if (decompressed < 8)
        {
          dsk_set_error (error, "internal error: data too short");
          return DSK_FALSE;
        }
    }

  uint32_t d[2];
  memcpy (d, o, 8);
  d[0] = UINT32_TO_LITTLE_ENDIAN (d[0]);
  d[1] = UINT32_TO_LITTLE_ENDIAN (d[1]);
  unsigned kv = d[0] + d[1];

  r->base.key_length = d[0];
  r->base.value_length = d[1];
  if (decompressed >= 8 + kv)
    {
      /* no copying / allocation required */
      r->base.key_data = o + 8;
      r->base.value_data = r->base.key_data + d[0];
      o += 8 + kv;
      r->outbuf_bytes_used = o - r->outbuf;
    }
  else
    {
      /* ensure scratch space is big enough */
      if (r->scratch_alloced < kv)
        {
          unsigned new_alloced = r->scratch_alloced;
          if (new_alloced == 0)
            new_alloced = 256;
          while (new_alloced < kv)
            new_alloced *= 2;
          r->scratch = dsk_realloc (r->scratch, new_alloced);
          r->scratch_alloced = new_alloced;
        }

      /* copy in already uncompressed data */
      memcpy (r->scratch, o + 8, decompressed - 8);
      o = r->outbuf;

      /* decompress until key and value are filled in. */
      r->decompressor.next_out = r->scratch + (decompressed - 8);
      r->decompressor.avail_out = kv - (decompressed - 8);
      while (r->decompressor.avail_out > 0)
        {
          if (r->decompressor.avail_in == 0)
            {
              /* Read into inbuf */
              if (r->fd < 0)
                {
                  dsk_set_error (error, "partial data at end of buffer");
                  return DSK_FALSE;
                }
              int read_rv = read (r->fd, r->inbuf, READER_INBUF_SIZE);
              if (read_rv < 0)
                {
                  if (errno == EINTR || errno == EAGAIN)
                    continue;
                  dsk_set_error (error, "error reading from file: %s",
                                 strerror (errno));
                  return DSK_FALSE;
                }
              else if (read_rv == 0)
                {
                  close (r->fd);
                  r->fd = -1;
                }
              else
                {
                  r->decompressor.next_in = r->inbuf;
                  r->decompressor.avail_in = read_rv;
                }
            }
          zrv = inflate (&r->decompressor, 0);
          if (zrv != Z_OK && zrv != Z_STREAM_END && zrv != Z_BUF_ERROR)
            goto zlib_error;
          if (r->fd < 0 && r->decompressor.avail_out > 0)
            {
              dsk_set_error (error, "partial data at end of buffer");
              return DSK_FALSE;
            }
        }

      r->decompressor.next_out = r->outbuf;
      r->decompressor.avail_out = READER_OUTBUF_SIZE;
      r->outbuf_bytes_used = 0;
      r->base.key_data = r->scratch;
      r->base.value_data = r->scratch + d[0];
    }
  return DSK_TRUE;

zlib_error:
  dsk_set_error (error, "zlib's inflate returned error code %d (%s)", zrv,
                 r->decompressor.msg);

  return DSK_FALSE;
}

void
dsk_table_file_reader_destroy (DskTableFileReader *reader)
{
  Reader *r = (Reader *) reader;
  if (r->fd)
    close (r->fd);
  inflateEnd (&r->decompressor);
  dsk_free (r->scratch);
  dsk_free (r);
}


/* --- Metadata parsing --- */
#define MAX_METADATA_SIZE    1024
typedef struct _DskTableFileMetadata DskTableFileMetadata;
struct _DskTableFileMetadata
{
  uint64_t entries;
  uint64_t key_bytes;
  uint64_t data_bytes;
  uint64_t compressed_size;
  unsigned n_index_levels;
  unsigned fanout;
};
static dsk_boolean
dsk_table_file_metadata_parse (const DskTableFileOptions *options,
                               DskTableFileMetadata      *out,
                               DskError                 **error)
{
  unsigned basefilename_len = strlen (options->base_filename);
  char *filename_buf = dsk_malloc (basefilename_len + 16);
  int fd;
  struct stat stat_buf;
  memcpy (filename_buf, options->base_filename, basefilename_len);

  /* Read metadata (needed for the number of index levels) */
  strcpy (filename_buf + basefilename_len, ".metadata");
retry_metadata_open:
  if (options->openat_fd < 0)
    fd = open (filename_buf, O_RDONLY);
  else
    fd = openat (options->openat_fd, filename_buf, O_RDONLY);
  if (fd < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        goto retry_metadata_open;
      dsk_set_error (error, "error opening %s: %s", filename_buf,
                     strerror (errno));
      dsk_free (filename_buf);
      return DSK_FALSE;
    }
  if (fstat (fd, &stat_buf) < 0)
    {
      dsk_set_error (error, "error finding size of %s: %s", filename_buf,
                     strerror (errno));
      dsk_free (filename_buf);
      close (fd);
      return DSK_FALSE;
    }
  if (stat_buf.st_size > MAX_METADATA_SIZE)
    {
      dsk_set_error (error, "table-file metadata too long? (%s)", filename_buf);
      dsk_free (filename_buf);
      close (fd);
      return DSK_FALSE;
    }

  /* split into lines */
  char *metadata_buf;
  char *metadata_buf_end;
  metadata_buf = dsk_malloc (stat_buf.st_size);
  if (!read_from_fd (fd, stat_buf.st_size, metadata_buf, error))
    {
      dsk_free (filename_buf);
      dsk_free (metadata_buf);
      return DSK_FALSE;
    }
  metadata_buf_end = metadata_buf + stat_buf.st_size;
  char *at = metadata_buf;
  unsigned got = 0;
  while (at < metadata_buf_end)
    {
      char *nl = memchr (at, '\n', metadata_buf_end - at);
      if (nl == NULL)
        {
          dsk_set_error (error, "unended line in metadata file (%s)", filename_buf);
          dsk_free (metadata_buf);
          dsk_free (filename_buf);
          return DSK_FALSE;
        }
      *nl = 0;
      
      if (strncmp (at, "entries: ", 9) == 0)
        {
          out->entries = strtoull (at + 9, NULL, 10);
          got |= 1;
        }
      else if (strncmp (at, "key-bytes: ", 11) == 0)
        {
          out->key_bytes = strtoull (at + 11, NULL, 10);
          got |= 2;
        }
      else if (strncmp (at, "data-bytes: ", 12) == 0)
        {
          out->data_bytes = strtoull (at + 12, NULL, 10);
          got |= 4;
        }
      else if (strncmp (at, "compressed-size: ", 17) == 0)
        {
          out->compressed_size = strtoull (at + 17, NULL, 10);
          got |= 8;
        }
      else if (strncmp (at, "n-index-levels: ", 16) == 0)
        {
          out->n_index_levels = strtoul (at + 16, NULL, 10);
          got |= 16;
        }
      else if (strncmp (at, "fanout: ", 8) == 0)
        {
          out->fanout = strtoul (at + 8, NULL, 10);
          got |= 32;
        }
      else
        {
          dsk_set_error (error, "unparsable line '%s' in %s", at, filename_buf);
          dsk_free (metadata_buf);
          dsk_free (filename_buf);
          return DSK_FALSE;
        }
    }
  if (got != 63)
    {
      dsk_set_error (error, "missing fields in %s:%s%s%s%s%s%s",
                     filename_buf,
                     (got&1) ? "" : " entries",
                     (got&2) ? "" : " key-bytes",
                     (got&4) ? "" : " data-bytes",
                     (got&8) ? "" : " compressed-size",
                     (got&16) ? "" : " n-index-levels",
                     (got&32) ? "" : " fanout");
      dsk_free (metadata_buf);
      dsk_free (filename_buf);
      return DSK_FALSE;
    }

  dsk_free (metadata_buf);
  dsk_free (filename_buf);
  return DSK_TRUE;
}

/* --- Searcher --- */
typedef struct _SeekerIndex SeekerIndex;
struct _SeekerIndex
{
  int index_fd, heap_fd;
};
struct _DskTableFileSeeker
{
  unsigned n_levels;
  unsigned fanout;
  SeekerIndex *index_levels;
  int compressed_data_fd;
};

static void
seeker_free_index_level_array (unsigned n_levels, SeekerIndex *index_levels)
{
  unsigned i;
  for (i = 0; i < n_levels; i++)
    {
      close (index_levels[i].heap_fd);
      close (index_levels[i].index_fd);
    }
  dsk_free (index_levels);
}

DskTableFileSeeker *dsk_table_file_seeker_new (DskTableFileOptions *options,
                                               DskError           **error)
{
  DskTableFileMetadata metadata;
  unsigned i;

  unsigned basefilename_len = strlen (options->base_filename);
  char *filename_buf = dsk_malloc (basefilename_len + 32);
  SeekerIndex *index_levels;
  int openat_fd = options->openat_fd < 0 ? AT_FDCWD : options->openat_fd;
  if (!dsk_table_file_metadata_parse (options, &metadata, error))
    return NULL;
  memcpy (filename_buf, options->base_filename, basefilename_len);

  /* Open the indices */
  unsigned n_index_levels = metadata.n_index_levels;
  index_levels = dsk_malloc (sizeof (SeekerIndex) * n_index_levels);
  for (i = 0; i < n_index_levels; i++)
    {
      snprintf (filename_buf + basefilename_len, 32, ".%03ui", i);
      index_levels[i].index_fd = openat (openat_fd, filename_buf, O_RDONLY);
      if (index_levels[i].index_fd < 0)
        {
          dsk_set_error (error, "opening index %s failed: %s",
                         filename_buf, strerror (errno));
          seeker_free_index_level_array (i, index_levels);
          dsk_free (filename_buf);
          return NULL;
        }
      snprintf (filename_buf + basefilename_len, 32, ".%03uh", i);
      index_levels[i].heap_fd = openat (openat_fd, filename_buf, O_RDONLY);
      if (index_levels[i].heap_fd < 0)
        {
          dsk_set_error (error, "opening heap %s failed: %s",
                         filename_buf, strerror (errno));
          close (index_levels[i].index_fd);
          seeker_free_index_level_array (i, index_levels);
          dsk_free (filename_buf);
          return NULL;
        }
    }

  /* Open the compressed data fd */
  filename_buf[basefilename_len] = 0;
  int compressed_data_fd;
  compressed_data_fd = openat (openat_fd, filename_buf, O_RDONLY);
  if (compressed_data_fd < 0)
    {
      dsk_set_error (error, "opening compressed-file %s failed: %s",
                     filename_buf, strerror (errno));
      seeker_free_index_level_array (n_index_levels, index_levels);
      dsk_free (filename_buf);
      return NULL;
    }

  DskTableFileSeeker *rv = dsk_malloc (sizeof (DskTableFileSeeker));
  rv->n_levels = metadata.n_index_levels;
  rv->index_levels = index_levels;
  rv->compressed_data_fd = compressed_data_fd;
  return rv;
}


dsk_boolean
dsk_table_file_seeker_find_first (DskTableFileSeeker    *seeker,
                                  DskTableSeekerTestFunc func,
                                  void                  *func_data,
                                  DskError             **error);
 
#if 0
dsk_boolean
dsk_table_file_seeker_find_last  (DskTableFileSeeker    *seeker,
                                  DskTableSeekerTestFunc func,
                                  void                  *func_data,
                                  DskError             **error);
#endif

/* Information about our current location. */
dsk_boolean  dsk_table_file_seeker_peek_cur   (DskTableFileSeeker    *seeker,
					       unsigned              *len_out,
					       const uint8_t        **data_out);
dsk_boolean  dsk_table_file_seeker_peek_index (DskTableFileSeeker    *seeker,
					       uint64_t              *index_out);

/* Advance forward in the file. */
dsk_boolean  dsk_table_file_seeker_advance    (DskTableFileSeeker    *seeker);


void         dsk_table_file_seeker_destroy    (DskTableFileSeeker    *seeker)
{
  ...
  seeker_free_index_level_array (seeker->n_index_levels, seeker->index_levels);
  ...
  dsk_free (seeker);
}
