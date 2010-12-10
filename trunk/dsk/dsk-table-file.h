

typedef struct _DskTableFileOptions DskTableFileOptions;
struct _DskTableFileOptions
{
  unsigned index_fanout;
  int openat_fd;
  unsigned gzip_level;
  const char *base_filename;
};

/* --- Writer --- */
DskTableFileWriter *dsk_table_file_writer_new (DskTableFileOptions *options,
                                               DskError           **error);
dsk_boolean dsk_table_file_write (DskTableFileWriter *writer,
                                  unsigned            key_length,
			          const uint8_t      *key_data,
                                  unsigned            value_length,
			          const uint8_t      *value_data,
			          DskError          **error);
dsk_boolean dsk_table_file_writer_close   (DskTableFileWriter *writer,
                                           DskError           **error);
void        dsk_table_file_writer_destroy (DskTableFileWriter *writer);

/* --- Reader --- */
DskTableFileReader *dsk_table_file_reader_new (DskTableFileOptions *options,
                                               DskError           **error);
dsk_boolean dsk_table_file_read  (DskTableFileReader *reader,
                                  unsigned           *key_length_out,
			          const uint8_t     **key_data_out,
                                  unsigned           *value_length_out,
			          const uint8_t     **value_data_out,
			          DskError          **error);
dsk_boolean dsk_table_file_reader_close   (DskTableFileReader *reader,
                                           DskError           **error);
void        dsk_table_file_reader_destroy (DskTableFileReader *reader);

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
