typedef struct _DskBuffer DskBuffer;
typedef struct _DskBufferFragment DskBufferFragment;

struct _DskBufferFragment
{
  DskBufferFragment    *next;
  uint8_t              *buf;
  unsigned              buf_max_size;	/* allocation size of buf */
  unsigned              buf_start;	/* offset in buf of valid data */
  unsigned              buf_length;	/* length of valid data in buf */
  
  dsk_boolean           is_foreign;
  DskDestroyNotify      destroy;
  void                 *destroy_data;
};

struct _DskBuffer
{
  unsigned              size;

  DskBufferFragment    *first_frag;
  DskBufferFragment    *last_frag;
};

#define DSK_BUFFER_STATIC_INIT		{ 0, NULL, NULL }


void     dsk_buffer_init                (DskBuffer       *buffer);

unsigned dsk_buffer_read                (DskBuffer    *buffer,
                                         unsigned      max_length,
                                         void         *data);
unsigned dsk_buffer_peek                (const DskBuffer* buffer,
                                         unsigned      max_length,
                                         void         *data);
int      dsk_buffer_discard             (DskBuffer    *buffer,
                                         unsigned      max_discard);
char    *dsk_buffer_read_line           (DskBuffer    *buffer);

char    *dsk_buffer_parse_string0       (DskBuffer    *buffer);
                        /* Returns first char of buffer, or -1. */
int      dsk_buffer_peek_char           (const DskBuffer *buffer);
int      dsk_buffer_read_char           (DskBuffer    *buffer);

/* 
 * Appending to the buffer.
 */
void     dsk_buffer_append              (DskBuffer    *buffer, 
                                         unsigned      length,
                                         const void   *data);
void     dsk_buffer_append_string       (DskBuffer    *buffer, 
                                         const char   *string);
void     dsk_buffer_append_byte         (DskBuffer    *buffer, 
                                         uint8_t       character);
void     dsk_buffer_append_repeated_byte(DskBuffer    *buffer, 
                                         unsigned      count,
                                         uint8_t       character);
#define dsk_buffer_append_zeros(buffer, count) \
  dsk_buffer_append_repeated_byte ((buffer), 0, (count))


void     dsk_buffer_append_string0      (DskBuffer    *buffer,
                                         const char   *string);

void     dsk_buffer_append_foreign      (DskBuffer    *buffer,
					 unsigned      length,
                                         const void   *data,
					 DskDestroyNotify destroy,
					 void         *destroy_data);

void     dsk_buffer_printf              (DskBuffer    *buffer,
					 const char   *format,
					 ...) DSK_GNUC_PRINTF(2,3);
void     dsk_buffer_vprintf             (DskBuffer    *buffer,
					 const char   *format,
					 va_list       args);

/* Take all the contents from src and append
 * them to dst, leaving src empty.
 */
unsigned dsk_buffer_drain               (DskBuffer    *dst,
                                         DskBuffer    *src);

/* Like `drain', but only transfers some of the data. */
unsigned dsk_buffer_transfer            (DskBuffer    *dst,
                                         DskBuffer    *src,
					 unsigned      max_transfer);

/* file-descriptor mucking */
int      dsk_buffer_writev              (DskBuffer       *read_from,
                                         int              fd);
int      dsk_buffer_readv               (DskBuffer       *write_to,
                                         int              fd);

/* This deallocates memory used by the buffer-- you are responsible
 * for the allocation and deallocation of the DskBuffer itself. */
void     dsk_buffer_clear               (DskBuffer    *to_destroy);

/* Free all unused buffer fragments. */
void     _dsk_buffer_cleanup_recycling_bin ();


/* misc */
int dsk_buffer_index_of(DskBuffer *buffer, char char_to_find);
