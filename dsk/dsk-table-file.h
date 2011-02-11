
typedef struct _DskTableFileWriter DskTableFileWriter;
typedef struct _DskTableFileReader DskTableFileReader;
typedef struct _DskTableFileSeeker DskTableFileSeeker;
typedef struct _DskTableFileInterface DskTableFileInterface;
typedef struct _DskTableFileCompressor DskTableFileCompressor;

struct _DskTableFileWriter
{
  dsk_boolean (*write)  (DskTableFileWriter *writer,
                         unsigned            key_length,
                         const uint8_t      *key_data,
                         unsigned            value_length,
                         const uint8_t      *value_data,
                         DskError          **error);
  dsk_boolean (*close)  (DskTableFileWriter *writer,
                         DskError          **error);
  void        (*destroy)(DskTableFileWriter *writer);
};

struct _DskTableFileReader
{
  /* Readonly public data */
  dsk_boolean at_eof;
  unsigned key_length;
  unsigned value_length;
  const uint8_t *key_data;
  const uint8_t *value_data;

  /* Virtual functions */
  dsk_boolean (*advance)     (DskTableFileReader *reader,
                              DskError          **error);
  void        (*destroy)     (DskTableFileReader *reader);
};

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

struct _DskTableFileSeeker
{
  dsk_boolean (*find)      (DskTableFileSeeker    *seeker,
                            DskTableSeekerFindFunc func,
                            void                  *func_data,
                            DskTableFileFindMode   mode,
                            unsigned              *key_len_out,
                            const uint8_t        **key_data_out,
                            unsigned              *value_len_out,
                            const uint8_t        **value_data_out,
                            DskError             **error);
 
 
  DskTableFileReader *
             (*find_reader)(DskTableFileSeeker    *seeker,
                            DskTableSeekerFindFunc func,
                            void                  *func_data,
                            DskError             **error);
 
  dsk_boolean (*index)     (DskTableFileSeeker    *seeker,
                            uint64_t               index,
                            unsigned              *key_len_out,
                            const void           **key_data_out,
                            unsigned              *value_len_out,
                            const void           **value_data_out,
                            DskError             **error);
 
  DskTableFileReader *
            (*index_reader)(DskTableFileSeeker    *seeker,
                            uint64_t               index,
                            DskError             **error);
 

  void         (*destroy)  (DskTableFileSeeker    *seeker);

};


struct _DskTableFileInterface
{
  DskTableFileWriter *(*new_writer) (DskTableFileInterface   *iface,
                                     int                      openat_fd,
                                     const char              *base_filename,
                                     DskError               **error);
  DskTableFileReader *(*new_reader) (DskTableFileInterface   *iface,
                                     int                      openat_fd,
                                     const char              *base_filename,
                                     DskError               **error);
  DskTableFileSeeker *(*new_seeker) (DskTableFileInterface   *iface,
                                     int                      openat_fd,
                                     const char              *base_filename,
                                     DskError               **error);
  void                (*destroy)    (DskTableFileInterface   *iface);
};

  
extern DskTableFileInterface dsk_table_file_interface_default;

DskTableFileInterface *dsk_table_file_interface_new (DskTableFileCompressor *,
                                                     unsigned    n_index_levels,
                                                     const unsigned *fanouts);

extern DskTableFileInterface dsk_table_file_interface_trivial;

struct _DskTableFileCompressor
{
  /* Fails iff the output buffer is too short. */
  dsk_boolean (*compress)   (DskTableFileCompressor *compressor,
                             unsigned                in_len,
                             const uint8_t          *in_data,
                             unsigned               *out_len_inout,
                             uint8_t                *out_data);

  /* Fails on corrupt data, or if the output buffer is too small. */
  dsk_boolean (*decompress) (DskTableFileCompressor *compressor,
                             unsigned                in_len,
                             const uint8_t          *in_data,
                             unsigned               *out_len_inout,
                             uint8_t                *out_data);
};
