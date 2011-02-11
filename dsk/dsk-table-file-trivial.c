#include "dsk.h"
#include <endian.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "dsk-table-helper.h"

#if (BYTE_ORDER == LITTLE_ENDIAN)
#define UINT64_TO_LE(val)  (val)
#define UINT32_TO_LE(val)  (val)
#elif (BYTE_ORDER == BIG_ENDIAN)
#define UINT64_TO_LE(val)  bswap64(val)
#define UINT32_TO_LE(val)  bswap32(val)
#else
#error unknown endianness
#endif

/* synonyms provided for clarity */
#define UINT64_FROM_LE UINT64_TO_LE
#define UINT32_FROM_LE UINT32_TO_LE

/* NOTE: we assume that this structure can be fwritten on little-endian
   architectures.  on big-endian, swaps are required.
   in all cases, the data must be packed in the order given. */
typedef struct _IndexEntry IndexEntry;
struct _IndexEntry
{
  uint64_t heap_offset;
  uint32_t key_length;
  uint32_t value_length;
};
#define SIZEOF_INDEX_ENTRY sizeof(IndexEntry)


/* === Writer === */
typedef struct _TableFileTrivialWriter TableFileTrivialWriter;
struct _TableFileTrivialWriter
{
  DskTableFileWriter base_instance;
  FILE *index_fp, *heap_fp;
  uint64_t heap_fp_offset;
};


static dsk_boolean 
table_file_trivial_writer__write  (DskTableFileWriter *writer,
                                   unsigned            key_length,
                                   const uint8_t      *key_data,
                                   unsigned            value_length,
                                   const uint8_t      *value_data,
                                   DskError          **error)
{
  TableFileTrivialWriter *wr = (TableFileTrivialWriter *) writer;
  
  IndexEntry ie;
  ie.heap_offset = UINT64_TO_LE (wr->heap_fp_offset);
  ie.key_length = UINT32_TO_LE (key_length);
  ie.value_length = UINT32_TO_LE (value_length);
  if (fwrite (&ie, 16, 1, wr->index_fp) != 1)
    {
      dsk_set_error (error, "error writing entry to index file: %s",
                     strerror (errno));
      return DSK_FALSE;
    }
  if (key_length != 0
   && fwrite (key_data, key_length, 1, wr->heap_fp) != 1)
    {
      dsk_set_error (error, "error writing key to heap file: %s",
                     strerror (errno));
      return DSK_FALSE;
    }
  if (value_length != 0
   && fwrite (value_data, value_length, 1, wr->heap_fp) != 1)
    {
      dsk_set_error (error, "error writing value to heap file: %s",
                     strerror (errno));
      return DSK_FALSE;
    }

  wr->heap_fp_offset += key_length + value_length;
  return DSK_TRUE;
}

static dsk_boolean 
table_file_trivial_writer__close  (DskTableFileWriter *writer,
                                   DskError          **error)
{
  TableFileTrivialWriter *wr = (TableFileTrivialWriter *) writer;
  if (fclose (wr->index_fp) != 0)
    {
      dsk_set_error (error, "error closing index file: %s",
                     strerror (errno));
      wr->index_fp = NULL;
      return DSK_FALSE;
    }
  wr->index_fp = NULL;
  if (fclose (wr->heap_fp) != 0)
    {
      dsk_set_error (error, "error closing heap file: %s",
                     strerror (errno));
      wr->heap_fp = NULL;
      return DSK_FALSE;
    }
  wr->heap_fp = NULL;
  return DSK_TRUE;
}

static void        
table_file_trivial_writer__destroy(DskTableFileWriter *writer)
{
  TableFileTrivialWriter *wr = (TableFileTrivialWriter *) writer;
  if (wr->index_fp)
    fclose (wr->index_fp);
  if (wr->heap_fp)
    fclose (wr->heap_fp);
  dsk_free (wr);
}

static DskTableFileWriter *
table_file_trivial__new_writer (DskTableFileInterface   *iface,
                                int                      openat_fd,
                                const char              *base_filename,
                                DskError               **error)
{
  DSK_UNUSED (iface);
  TableFileTrivialWriter wr =
  {
    {
      table_file_trivial_writer__write,
      table_file_trivial_writer__close,
      table_file_trivial_writer__destroy
    },
    NULL, NULL, 0ULL
  };
  int fd;
  fd = dsk_table_helper_openat (openat_fd, base_filename, ".index",
                                O_CREAT|O_TRUNC|O_WRONLY, 0666,
                                error);
  if (fd < 0)
    return DSK_FALSE;
  wr.index_fp = fdopen (fd, "wb");
  dsk_assert (wr.index_fp);
  fd = dsk_table_helper_openat (openat_fd, base_filename, ".heap",
                                O_CREAT|O_TRUNC|O_WRONLY, 0666,
                                error);
  if (fd < 0)
    {
      fclose (wr.index_fp);
      return DSK_FALSE;
    }
  wr.heap_fp = fdopen (fd, "wb");
  dsk_assert (wr.heap_fp);

  return dsk_memdup (sizeof (wr), &wr);
}

/* === Reader === */
typedef struct _TableFileTrivialReader TableFileTrivialReader;
struct _TableFileTrivialReader
{
  DskTableFileReader base_instance;
  FILE *index_fp, *heap_fp;
  uint64_t next_heap_offset;
  unsigned slab_alloced;
  uint8_t *slab;
};

static dsk_boolean
read_next_index_entry (TableFileTrivialReader *reader,
                       DskError              **error)
{
  IndexEntry ie;
  size_t nread;
  nread = fread (&ie, 1, sizeof (IndexEntry), reader->index_fp);
  if (nread == 0)
    {
      /* at EOF */
      reader->base_instance.at_eof = DSK_TRUE;
      return DSK_TRUE;
    }
  else if (nread < sizeof (IndexEntry))
    {
      if (ferror (reader->index_fp))
        {
          /* actual error */
          dsk_set_error (error, "error reading index file: %s",
                         strerror (errno));
        }
      else
        {
          /* file truncated */
          dsk_set_error (error,
                         "index file truncated (partial record encountered)");
        }
      return DSK_FALSE;
    }
  ie.heap_offset = UINT64_FROM_LE (ie.heap_offset);
  ie.key_length = UINT32_FROM_LE (ie.key_length);
  ie.value_length = UINT32_FROM_LE (ie.value_length);

  if (ie.heap_offset != reader->next_heap_offset)
    {
      dsk_set_error (error, "heap offset from index file was %llu instead of expected %llu",
                     ie.heap_offset, reader->next_heap_offset);
      return DSK_FALSE;
    }

  unsigned kv_len = ie.key_length + ie.value_length;
  if (reader->slab_alloced < kv_len)
    {
      unsigned new_alloced = reader->slab_alloced;
      if (new_alloced == 0)
        new_alloced = 16;
      while (new_alloced < kv_len)
        new_alloced *= 2;
      reader->slab = dsk_realloc (reader->slab, new_alloced);
      reader->slab_alloced = new_alloced;
    }
  if (kv_len != 0 && fread (reader->slab, kv_len, 1, reader->heap_fp) != 1)
    {
      dsk_set_error (error, "error reading key/value from heap file");
      return DSK_FALSE;
    }
  reader->base_instance.key_length = ie.key_length;
  reader->base_instance.value_length = ie.value_length;
  reader->base_instance.key_data = reader->slab;
  reader->base_instance.value_data = reader->slab + ie.key_length;
  reader->next_heap_offset += kv_len;
  return DSK_TRUE;
}

static dsk_boolean
table_file_trivial_reader__advance (DskTableFileReader *reader,
                                    DskError          **error)
{
  return read_next_index_entry ((TableFileTrivialReader *) reader, error);
}

static void
table_file_trivial_reader__destroy (DskTableFileReader *reader)
{
  TableFileTrivialReader *t = (TableFileTrivialReader *) reader;
  fclose (t->index_fp);
  fclose (t->heap_fp);
  dsk_free (t);
}


static DskTableFileReader *
table_file_trivial__new_reader (DskTableFileInterface   *iface,
                                int                      openat_fd,
                                const char              *base_filename,
                                DskError               **error)
{
  DSK_UNUSED (iface);
  TableFileTrivialReader reader = {
    {
      DSK_FALSE, 0, 0, NULL, NULL,

      table_file_trivial_reader__advance,
      table_file_trivial_reader__destroy
    },
    NULL, NULL, 0ULL, 0, NULL
  };
  int fd = dsk_table_helper_openat (openat_fd, base_filename, ".index",
                                    O_RDONLY, 0, error);
  if (fd < 0)
    return NULL;

  reader.index_fp = fdopen (fd, "rb");
  dsk_assert (reader.index_fp != NULL);
  fd = dsk_table_helper_openat (openat_fd, base_filename, ".heap",
                                O_RDONLY, 0, error);
  if (fd < 0)
    {
      fclose (reader.index_fp);
      return NULL;
    }
  reader.heap_fp = fdopen (fd, "rb");
  dsk_assert (reader.heap_fp != NULL);

  if (!read_next_index_entry (&reader, error))
    {
      fclose (reader.index_fp);
      fclose (reader.heap_fp);
      return NULL;
    }
  return dsk_memdup (sizeof (reader), &reader);
}

/* === Seeker === */
typedef struct _TableFileTrivialSeeker TableFileTrivialSeeker;
struct _TableFileTrivialSeeker
{
  DskTableFileWriter base_instance;
  int index_fd, heap_fd;
};

static inline dsk_boolean
check_index_entry_lengths (const IndexEntry *ie)
{
  return ie->key_length < (1<<30)
      && ie->value_length < (1<<30);
}

static inline dsk_boolean
read_index_entry (DskTableFileSeeker *seeker,
                  uint64_t            index,
                  IndexEntry         *out,
                  DskError          **error)
{
  ssize_t nread;
  TableFileTrivialSeeker *s = (TableFileTrivialSeeker *) seeker;
  IndexEntry ie;
  nread = dsk_table_helper_pread (s->index_fd, &ie,
                                  SIZEOF_INDEX_ENTRY,
                                  index * SIZEOF_INDEX_ENTRY);
  if (nread < 0)
    {
      dsk_set_error (error, "error reading index entry %llu: %s",
                     index, strerror (errno));
      return DSK_FALSE;
    }
  if (nread < (int) SIZEOF_INDEX_ENTRY)
    {
      dsk_set_error (error, "too short reading index entry %llu", index);
      return DSK_FALSE;
    }
  ie.heap_offset = UINT64_FROM_LE (ie.heap_offset);
  ie.key_length = UINT32_FROM_LE (ie.key_length);
  ie.value_length = UINT32_FROM_LE (ie.value_length);

  if (!check_index_entry_lengths (&ie))
    {
      dsk_set_error (error, "corrupted index entry %llu", index);
      return DSK_FALSE;
    }
  *out = ie;
  return DSK_TRUE;
}

static dsk_boolean
run_cmp (TableFileTrivialSeeker *s,
         DskTableSeekerFindFunc func,
         uint64_t               index,
         void                  *func_data,
         int                   *cmp_rv_out,
         IndexEntry            *ie_out,
         DskError             **error)
{
  if (!read_index_entry ((DskTableFileSeeker*)s, index, ie_out, error))
    return DSK_FALSE;
  if (ie_out->key_length != 0)
    {
      if (s->slab_alloced < ie_out->key_length)
        {
          ...
        }
      ssize_t nread = dsk_table_helper_pread (s->heap_fd, s->slab,
                                              ie_out->key_length,
                                              ie_out->heap_offset);
      if (nread < ie_out->key_length)
        {
          ...
        }
    }
  return func (ie_out->key_length, s->slab, func_data);
}

static dsk_boolean
find_index (DskTableFileSeeker    *seeker,
            DskTableSeekerFindFunc func,
            void                  *func_data,
            DskTableFileFindMode   mode,
            uint64_t              *index_out,
            DskError             **error)
{
  TableFileTrivialSeeker *s = (TableFileTrivialSeeker *) seeker;
  uint64_t start = 0, n = s->count;
  while (count > 0)
    {
      int cmp_rv;
      uint64_t mid = start + n / 2;
      if (!run_cmp (s, func, func_data, mid, &cmp_rv, error))
        return DSK_FALSE;
      if (cmp_rv < 0)
        {
          n = mid - start;
        }
      else if (cmp_rv > 0)
        {
          n = (start + n) - (mid + 1);
          start = mid + 1;
        }
      else /* cmp_rv == 0 */
        {
          switch (mode)
            {
            case DSK_TABLE_FILE_FIND_FIRST:
              {
                n = mid + 1 - start;

                /* bsearch, knowing that (start+n-1) is in
                   the range of elements to return. */
                while (n > 1)
                  {
                    mid2 = start + n / 2;
                    if (!run_cmp (s, func, func_data, mid2, &cmp_rv, error))
                      return DSK_FALSE;
                    dsk_assert (cmp_rv <= 0);
                    if (cmp_rv == 0)
                      {
                        n = mid2 + 1 - start;
                      }
                    else
                      {
                        n = (start + n) - (mid2 + 1);
                        start = mid2 + 1;
                      }
                  }
                *index_out = start;
                return DSK_TRUE;
              }

            case DSK_TABLE_FILE_FIND_ANY:
              {
                *index_out = mid;
                return DSK_TRUE;
              }
            case DSK_TABLE_FILE_FIND_LAST:
              {
                n = start + n - mid;
                start = mid;

                /* bsearch, knowing that start is in
                   the range of elements to return. */
                while (n > 1)
                  {
                    mid2 = start + n / 2;
                    if (!run_cmp (s, func, func_data, mid2, &cmp_rv, error))
                      return DSK_FALSE;
                    dsk_assert (cmp_rv <= 0);
                    if (cmp_rv == 0)
                      {
                        /* possible return value is the range
                           starting at mid2. */
                        n = start + n - mid2;
                        start = mid2;
                      }
                    else
                      {
                        n = mid2 - start;
                      }
                  }
                *index_out = start;
                return DSK_TRUE;
              }
            }
        }
    }

  /* not found. */
  return DSK_FALSE;
}

static dsk_boolean 
table_file_trivial_seeker__find  (DskTableFileSeeker    *seeker,
                                  DskTableSeekerFindFunc func,
                                  void                  *func_data,
                                  DskTableFileFindMode   mode,
                                  unsigned              *key_len_out,
                                  const uint8_t        **key_data_out,
                                  unsigned              *value_len_out,
                                  const uint8_t        **value_data_out,
                                  DskError             **error)
{
  uint64_t index;
  if (!find_index (seeker, func, func_data, mode, &index, error))
    return DSK_FALSE;

  if (key_len_out || key_data_out || value_len_out || value_data_out)
    {
      /* load key/value */
      ...
    }
  return DSK_TRUE;
}


static DskTableFileReader * 
table_file_trivial_seeker__find_reader(DskTableFileSeeker    *seeker,
                                       DskTableSeekerFindFunc func,
                                       void                  *func_data,
                                       DskError             **error)
{
  ...
}

static dsk_boolean 
table_file_trivial_seeker__index (DskTableFileSeeker    *seeker,
                                  uint64_t               index,
                                  unsigned              *key_len_out,
                                  const void           **key_data_out,
                                  unsigned              *value_len_out,
                                  const void           **value_data_out,
                                  DskError             **error)
{
  IndexEntry ie;
  if (!read_index_entry (seeker, index, &ie, error))
    return DSK_FALSE;
  kv_len = ie.key_length + ie.value_length;
  if (seeker->slab_alloced < kv_len)
    {
      unsigned new_size = seeker->slab_alloced;
      if (new_size == 0)
        new_size = 32;
      while (new_size < kv_len)
        new_size *= 2;
      seeker->slab_alloced = new_size;
      dsk_free (seeker->slab);
      seeker->slab = dsk_malloc (new_size);
    }
  if (kv_len > 0)
    {
      nread = dsk_table_helper_pread (seeker->heap_fd, seeker->slab,
                                      kv_len, ie.heap_offset);
      if (nread < 0)
        {
          dsk_set_error (error, "error reading heap entry %llu: %s",
                         index, strerror (errno));
          return DSK_FALSE;
        }
      if (nread < (int) kv_len)
        {
          dsk_set_error (error, "too short reading heap entry %llu (got %u of %u bytes)",
                         index, (unsigned) nread, kv_len);
          return DSK_FALSE;
        }
    }
  if (key_len_out != NULL)
    *key_len_out = ie.key_length;
  if (key_data_out != NULL)
    *key_data_out = seeker->slab;
  if (value_len_out != NULL)
    *value_len_out = ie.value_length;
  if (value_data_out != NULL)
    *value_data_out = seeker->slab + ie.key_length;
  return DSK_TRUE;
}

static DskTableFileReader * 
table_file_trivial_seeker__index_reader(DskTableFileSeeker    *seeker,
                                        uint64_t               index,
                                        DskError             **error)
{
  ...
}


static void         
table_file_trivial_seeker__destroy  (DskTableFileSeeker    *seeker)
{
  ...
}

static DskTableFileSeeker *
table_file_trivial__new_seeker (DskTableFileInterface   *iface,
                                int                      openat_fd,
                                const char              *base_filename,
                                DskError               **error);
{
  ...
}

/* No destructor required for the static interface */
#define table_file_trivial__destroy  NULL

DskTableFileInterface dsk_table_file_interface_trivial =
{
  table_file_trivial__new_writer,
  table_file_trivial__new_reader,
  table_file_trivial__new_seeker,
  table_file_trivial__destroy,
};
