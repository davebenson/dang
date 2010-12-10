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
#include "dsk.h"
#include "dsk-table-file.h"
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
                   O_CREAT|O_WRONLY|O_NOATIME, 0666);
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
alloc_index_level (DskTableFileWriter *writer,
                   IndexWriter       **out,
                   unsigned            index_level,
                   DskError          **error)
{
  int index_fd, heap_fd;
  char buf[10];
  snprintf (buf, sizeof (buf), ".%03uh", index_level);
  heap_fd = create_fd (writer, buf, error);
  if (index_fd < 0)
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
  writer->gzip_level = options->gzip_level;
  writer->compressed_data_fd = create_fd (writer, ".gz", error);
  if (writer->compressed_data_fd < 0)
    goto error_cleanup_2;
  writer->compressor.avail_in = 0;
  writer->compressor.next_in = NULL;
  writer->compressor.avail_out = COMPRESSOR_BUF_SIZE;
  writer->compressor.next_in = writer->out_buf;
  int zrv;
  zrv = deflateInit2 (&writer->compressor,
                      writer->gzip_level,
                      Z_DEFLATED,
                      31, 8,
                      Z_DEFAULT_STRATEGY);
  if (zrv != Z_OK)
    {
      dsk_warning ("deflateInit2 returned error");
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
  unsigned to_write = COMPRESSOR_BUF_SIZE - writer->compressor.avail_in;
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
retry_deflate:
  zrv = deflate (&writer->compressor, Z_FINISH);
  if (zrv == Z_STREAM_END)
    {
      uint64_t offset = writer->out_buf_file_offset
                      + (writer->compressor.next_out - writer->out_buf);
      writer->last_gzip_size = offset - writer->last_compressor_flush_offset;
      writer->last_compressor_flush_offset = offset;
      return DSK_TRUE;
    }
  if (zrv == Z_OK)
    {
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
      if (!write_index0_partial_entry (writer, key_length, key_data, error))
        return DSK_FALSE;

      /* Do we need to go the next level? */
      IndexWriter *index;
      if (--(writer->index_levels->entries_left) > 0)
        index = NULL;
      else
        index = writer->index_levels->up;

      while (index != NULL)
        {
          if (!write_index_entry (index, key_length, key_data, error))
            return DSK_FALSE;

          /* Do we need to go the next level? */
          if (--(index->entries_left) > 0)
            break;
          index = index->up;
        }
      if (index == NULL)
        {
          /* Create a new level of index */
          index = writer->index_levels;
          while (index->up)
            index = index->up;
          if (!alloc_index_level (writer, &index->up, 0, error))
            {
              /* Every index should begin with the first key. */
              if (!write_index_entry (index, writer->first_key_len, writer->first_key_data, error))
                return DSK_FALSE;

              /* Write index entry */
              if (!write_index_entry (index, key_length, key_data, error))
                return DSK_FALSE;
            }
        }
    }

  uint32_t le;
  le = UINT32_TO_LITTLE_ENDIAN (key_length);
  if (!table_file_write_data (writer, 4, (const uint8_t *) &le, error)
   || !table_file_write_data (writer, key_length, key_data, error))
    return DSK_FALSE;
  le = UINT32_TO_LITTLE_ENDIAN (value_length);
  if (!table_file_write_data (writer, 4, (const uint8_t *) &le, error)
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
    n_levels++;
  snprintf (metadata_buf, sizeof (metadata_buf),
            "entries: %llu\n"
            "key-bytes: %llu\n"
            "data-bytes: %llu\n"
            "compressed-size: %llu\n"
            "n-index-levels: %u\n",
            writer->total_n_entries,
            writer->total_key_bytes,
            writer->total_value_bytes,
            writer->out_buf_file_offset,
            n_levels);
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
      unlink_by_suffix (writer, ".gz");
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
  char *filename;
};

DskTableFileReader *
dsk_table_file_reader_new (DskTableFileOptions *options,
                           DskError           **error)
{
  Reader *rv;
  unsigned baselen = strlen (options->base_filename);
  char *name = dsk_malloc (baselen + 4);                /* .gz and NUL */
  memcpy (name, options->base_filename, baselen);
  strcpy (name + baselen, ".gz");
  int fd;
  if (options->openat_fd < 0)
    fd = open (name, O_RDONLY);
  else
    fd = openat (options->openat_fd, name, O_RDONLY);
  if (fd < 0)
    {
      dsk_set_error (error, "%s(): error reading %s: %s",
                     options->openat_fd < 0 ? "open" : "openat", name,
                     strerror (errno));
      dsk_free (name);
      return NULL;
    }
  rv = dsk_malloc (sizeof (Reader));
  memset (&rv->decompressor, 0, sizeof (z_stream));
  inflateInit (&rv->decompressor);
  rv->fd = fd;
  
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
  rv->filename = name;
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
              dsk_set_error (error, "error reading from %s: %s",
                             r->filename, strerror (errno));
              return DSK_FALSE;
            }
          else
            {
              r->decompressor.avail_in += read_rv;
              r->decompressor.next_in = r->decompressor.inbuf;
            }
          if (r->decompressor.avail_in > 0)
            {
              int zrv = inflate (&r->decompressor);
              decompressed = r->decompressor.next_out - r->outbuf;
              if (zrv != Z_OK && zrv != Z_STREAM_END && zrv != Z_BUF_ERROR)
                goto zlib_error;
            }
        }
      if (decompressed == 0)
        return DSK_FALSE;
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

  if (decompressed >= 8 + d[0] + d[1])
    {
      /* no copying / allocation required */
      ...
    }
  else
    {
      /* ensure scratch space is big enough.
         decompress until it is filled in. */
      ...
    }
  return DSK_TRUE;

zlib_error:
  dsk_set_error (error, "zlib's inflate returned error code %d", zrv);
  return DSK_FALSE;
}

dsk_boolean
dsk_table_file_reader_close   (DskTableFileReader *reader,
                               DskError           **error)
{
  ...
}

void
dsk_table_file_reader_destroy (DskTableFileReader *reader)
{
  ...
}


/* --- Searcher --- */
DskTableFileSeeker *dsk_table_file_seeker_new (DskTableFileOptions *options,
                                               DskError           **error);

/* Returns whether this value is in the set. */
typedef dsk_boolean (*DskTableSeekerTestFunc) (unsigned               len,
                                               const uint8_t         *data,
                                               void                  *user_data);

/* The comparison function should return TRUE if the value
   is greater than or equal to some threshold determined by func_data. */
dsk_boolean  dsk_table_file_seeker_find_first (DskTableFileSeeker    *seeker,
					       DskTableSeekerTestFunc func,
					       void                  *func_data,
                                               DskError             **error);

/* The comparison function should return TRUE if the value
   is less than or equal to some threshold determined by func_data. */
dsk_boolean  dsk_table_file_seeker_find_last  (DskTableFileSeeker    *seeker,
					       DskTableSeekerTestFunc func,
					       void                  *func_data,
                                               DskError             **error);

/* Information about our current location. */
dsk_boolean  dsk_table_file_seeker_peek_cur   (DskTableFileSeeker    *seeker,
					       unsigned              *len_out,
					       const uint8_t        **data_out);
dsk_boolean  dsk_table_file_seeker_peek_index (DskTableFileSeeker    *seeker,
					       uint64_t              *index_out);

/* Advance forward in the file. */
dsk_boolean  dsk_table_file_seeker_advance    (DskTableFileSeeker    *seeker);


void         dsk_table_file_seeker_destroy    (DskTableFileSeeker    *seeker);
