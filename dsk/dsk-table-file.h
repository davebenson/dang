

typedef struct _DskTableFileOptions DskTableFileOptions;
struct _DskTableFileOptions
{
  unsigned index_fanout;
  int openat_fd;
  unsigned gzip_level;
  const char *base_filename;
};
#define DSK_TABLE_FILE_OPTIONS_DEFAULT             \
{                                                  \
  16,           /* index_fanout */                 \
  -1,           /* openat_fd */                    \
  6,            /* gzip_level */                   \
  NULL                                             \
}
typedef struct _DskTableFileWriter DskTableFileWriter;
typedef struct _DskTableFileReader DskTableFileReader;
typedef struct _DskTableFileSeeker DskTableFileSeeker;

/* Return TRUE to ignore the error, and FALSE to pass the error back
   to the caller. */
typedef dsk_boolean (*DskTableErrorHandler) (DskError *error,
                                             void     *data);
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
struct _DskTableFileReader
{
  dsk_boolean at_eof;
  unsigned key_length;
  unsigned value_length;
  const uint8_t *key_data;
  const uint8_t *value_data;

  /*< private data follows >*/
};
DskTableFileReader *dsk_table_file_reader_new (DskTableFileOptions *options,
                                               DskError           **error);

/* Returns FALSE on EOF or error. */
dsk_boolean dsk_table_file_reader_advance     (DskTableFileReader *reader,
			                       DskError          **error);
void        dsk_table_file_reader_destroy     (DskTableFileReader *reader);

/* --- Searcher --- */
DskTableFileSeeker *dsk_table_file_seeker_new (DskTableFileOptions *options,
                                               DskError           **error);

/* Returns:
     -1 if the key is before the range we are searching for.
      0 if the key is in the range we are searching for.
     +1 if the key is after the range we are searching for.
 */
typedef int  (*DskTableSeekerFindFunc) (unsigned           key_len,
                                        const uint8_t     *key_data,
                                        void              *user_data);
typedef enum
{
  DSK_TABLE_FILE_FIND_FIRST,
  DSK_TABLE_FILE_FIND_ANY,
  DSK_TABLE_FILE_FIND_LAST
} DskTableFileFindMode;

/* The comparison function should return TRUE if the value
   is greater than or equal to some threshold determined by func_data.
   We return the first element for which the function returns TRUE.
   This function returns FALSE if:
     - no matching key is found (in which case *error will not be set)
     - an error occurs (corrupt data or disk error)
       (in which case *error will be set)
 */
dsk_boolean
dsk_table_file_seeker_find_full  (DskTableFileSeeker    *seeker,
                                  DskTableSeekerFindFunc func,
                                  void                  *func_data,
                                  DskTableFileFindMode   mode,
                                  unsigned              *key_len_out,
                                  const uint8_t        **key_data_out,
                                  unsigned              *value_len_out,
                                  const uint8_t        **value_data_out,
                                  DskError             **error);
dsk_boolean
dsk_table_file_seeker_find       (DskTableFileSeeker    *seeker,
                                  unsigned               key_len,
                                  const uint8_t         *key_data,
                                  unsigned              *value_len_out,
                                  const uint8_t        **value_data_out,
                                  DskError             **error);
 
 
DskTableFileReader *
dsk_table_file_seeker_find_reader(DskTableFileSeeker    *seeker,
                                  DskTableSeekerFindFunc func,
                                  void                  *func_data,
                                  DskError             **error);
 
dsk_boolean
dsk_table_file_seeker_index      (DskTableFileSeeker    *seeker,
                                  uint64_t               index,
                                  unsigned              *key_len_out,
                                  const void           **key_data_out,
                                  unsigned              *value_len_out,
                                  const void           **value_data_out,
                                  DskError             **error);
 
DskTableFileReader *
dsk_table_file_seeker_index_reader(DskTableFileSeeker    *seeker,
                                   uint64_t               index,
                                   DskError             **error);
 

void         dsk_table_file_seeker_destroy    (DskTableFileSeeker    *seeker);

