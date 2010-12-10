

typedef struct _DskTableFileOptions DskTableFileOptions;
struct _DskTableFileOptions
{
  unsigned index_fanout;
  int openat_fd;
  const char *base_filename;
};

/* --- Writer --- */
typedef struct _IndexWriter IndexWriter;
struct _IndexWriter
{
  FILE *index_fp;
  FILE *heap_fp;
  unsigned entries_left;
};

struct _DskTableFileWriter
{
  z_stream compressor;
  uint8_t out_buf[4096];
  unsigned fanout;
  unsigned entries_until_flush;
  dsk_boolean is_first;
  int compressed_data_fd;

  unsigned first_value_len;
  uint8_t *first_value_data;

  unsigned n_index_levels;
  IndexWriter *index_levels;
  unsigned index_levels_alloced;

  /* basefilename has 20 characters of padding to allow
     adding suffixes with snprintf(basefilename+basefilename_len, 20, ...); */
  char *basefilename;
  unsigned basefilename_len;   
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

DskTableFileWriter *dsk_table_file_writer_new (DskTableFileOptions *options,
                                               DskError           **error)
{
  DskTableFileWriter *writer = dsk_malloc (sizeof (DskTableFileWriter));
  writer->n_index_levels = 1;
  writer->index_levels_alloced = 8;
  writer->index_levels = dsk_malloc (sizeof (IndexWriter) * writer->index_levels_alloced);
  writer->index_levels[0].data_fp = ...;
  writer->index_levels[0].index_fp = ...;
  writer->index_levels[0].entries_left = options->fanout;
  writer->fanout = options->fanout;
  writer->entries_until_flush = 0;
  writer->is_first = DSK_TRUE;
  writer->openat_fd = options->openat_fd;
  if (writer->openat_fd < 0)
    writer->openat_fd = AT_FDCWD;
  writer->basefilename_len = strlen (options->base_filename);
  writer->basefilename = dsk_malloc (writer->basefilename_len + 30);
  if (writer->gzip_level > 0)
    writer->compressed_data_fd = create_fd (writer, ".data.gz");
  else
    writer->compressed_data_fd = create_fd (writer, ".data");

  memset (&writer->compressor, 0, sizeof (zstream));
  initDeflator (...);
  ...
  return writer;
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
write_index_entry (DskTableFileWriter *writer,
                   unsigned            index_level,
                   unsigned            key_length,
                   const uint8_t      *key_data,
                   DskError          **error)
{
  if (index_level == 0)
    {
      /* Data needs to include offset into (possibly) gzipped file */
      ...
    }
  else
    {
      /* Data just include index into this index's level of heap file. */
      ...
    }
}

dsk_boolean dsk_table_file_write (DskTableFileWriter *writer,
                                  unsigned            key_length,
			          const uint8_t      *key_data,
                                  unsigned            value_length,
			          const uint8_t      *value_data,
			          DskError          **error)
{
  guint32 le;

  if (writer->entries_until_flush == 0)
    {
      if (writer->is_first)
        {
          ...
          writer->is_first = DSK_FALSE;
        }
      else
        {
          /* Flush the existing inflator */
          ...
        }

      for (i = 0; i < writer->n_index_levels; i++)
        {
          /* Write index entry */
          if (!write_index_entry (writer, i, key_len, key_data, error))
            return DSK_FALSE;

          /* Do we need to go the next level? */
          if (--writer->index_levels[i].entries_left > 0)
            break;
          writer->index_levels[i].entries_left = writer->fanout;
        }
      if (i == writer->n_index_levels)
        {
          /* Create a new level of index */
          if (writer->n_index_levels == writer->index_levels_alloced)
            {
              writer->index_levels_alloced *= 2;
              writer->index_levels = dsk_realloc (writer->index_levels,
                                                  writer->index_levels_alloced * sizeof (IndexWriter));
            }
          writer->index_levels[writer->n_index_levels].data_fp = ...
          writer->index_levels[writer->n_index_levels].index_fp = ...;
          writer->index_levels[writer->n_index_levels].entries_left = writer->fanout;
          writer->n_index_levels++;

          /* Every index should begin with the first key. */
          if (!write_index_entry (writer, i, writer->first_key_len, writer->first_key_data, error))
            return DSK_FALSE;

          /* Write index entry */
          if (!write_index_entry (writer, i, key_len, key_data, error))
            return DSK_FALSE;
        }
    }

  le = UINT32_TO_LITTLE_ENDIAN (key_length);
  if (!table_file_write_data (writer, 4, (const uint8_t *) &le, error)
   || !table_file_write_data (writer, key_length, key_data, error))
    return DSK_FALSE;
  le = UINT32_TO_LITTLE_ENDIAN (value_data);
  if (!table_file_write_data (writer, 4, (const uint8_t *) &le, error)
   || !table_file_write_data (writer, value_length, value_data, error))
    return DSK_FALSE;
  return DSK_TRUE;
}

dsk_boolean dsk_table_file_writer_close   (DskTableFileWriter *writer,
                                           DskError           **error)
{
  if (!flush_compressed_data (writer, error))
    return DSK_FALSE;

  /* flush compression buffer to disk */
  ...

  writer->is_closed = DSK_TRUE;
}

void        dsk_table_file_writer_destroy (DskTableFileWriter *writer)
{
  /* Close all file descriptors */
  unsigned i;
  for (i = 0; i < writer->n_index_levels; i++)
    {
      fclose (writer->index_levels[i].index_fp);
      fclose (writer->index_levels[i].heap_fp);
    }
  dsk_free (writer->index_levels);
  close (writer->compressed_data_fd);

  deflateEnd (&writer->compressor);

  if (!writer->is_closed)
    {
      /* Delete all files if not closed. */
      ...
    }
  dsk_free (writer->basefilename);
  dsk_free (writer);
}

/* --- Reader --- */
DskTableFileReader *
dsk_table_file_reader_new (DskTableFileOptions *options,
                           DskError           **error)
{
  ...
}

dsk_boolean
dsk_table_file_read  (DskTableFileReader *reader,
                      unsigned           *key_length_out,
                      const uint8_t     **key_data_out,
                      unsigned           *value_length_out,
                      const uint8_t     **value_data_out,
                      DskError          **error)
{
  ...
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
