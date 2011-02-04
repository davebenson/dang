#include <endian.h>

#if (BYTE_ORDER == LITTLE_ENDIAN)
#define UINT64_TO_LE(val)  (val)
#define UINT32_TO_LE(val)  (val)
#elif (BYTE_ORDER == BIG_ENDIAN)
#define UINT64_TO_LE(val)  bswap64(val)
#define UINT32_TO_LE(val)  bswap32(val)
#else
#error unknown endianness
#endif

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
  
  struct IndexEntry ie;
  ie.heap_offset = UINT64_TO_LE (wr->heap_offset);
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

  wr->heap_offset += key_length + value_length;
  return DSK_TRUE;
}

static dsk_boolean 
table_file_trivial_writer__close  (DskTableFileWriter *writer,
                                   DskError          **error)
{
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
  TableFileTrivialWriter wr =
  {
    {
      table_file_trivial_writer__write,
      table_file_trivial_writer__close,
      table_file_trivial_writer__destroy
    },
    NULL, NULL, 0ULL
  };
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
};

static dsk_boolean
read_next_index_entry (TableFileTrivialReader *reader,
                       DskError              **error)
{
  IndexEntry ie;
  nread = fread (&ie, 1, sizeof (IndexEntry), reader.index_fp);
  if (nread == 0)
    {
      /* at EOF */
      reader.base_instance.at_eof = DSK_TRUE;
      goto do_return_reader_copy;
    }
  else if (nread < 0)
    {
      /* actual error */
      dsk_set_error (error, "error reading index file: %s",
                     strerror (errno));
      return DSK_FALSE;
    }
  else if (nread < (int) sizeof (IndexEntry))
    {
      /* file truncated */
      dsk_set_error (error, "index file truncated (partial record encountered)");
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
                                DskError               **error);
{
  TableFileTrivialReader reader = {
    {
      DSK_FALSE, 0, 0, NULL, NULL,

      table_file_trivial_reader__advance,
      table_file_trivial_reader__destroy
    },
    NULL, NULL
  };
  fd = dsk_table_helper_openat (openat_fd, base_filename, ".index",
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
do_return_reader_copy:
  return dsk_memdup (sizeof (reader), &reader);
}

/* === Seeker === */
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
  ...
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
  ...
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
