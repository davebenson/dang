
#include <alloca.h>
#define dang_alloca alloca
#include <assert.h>

typedef int dang_boolean;

#define DANG_UNUSED(param)     ((void)(param))

/* TODO: implement these */
#define DANG_UNLIKELY(cond)    (cond)
#define DANG_LIKELY(cond)      (cond)

#define DANG_ALIGN(offset, alignment) \
  (   ((offset) + (alignment) - 1) & (~((alignment) - 1))    )
#define DANG_N_ELEMENTS(static_array) \
  (sizeof(static_array) / sizeof((static_array)[0]))

#define DANG_UINT_TO_POINTER(i)   ((void*)(size_t)(unsigned)(i))
#define DANG_POINTER_TO_UINT(i)   ((unsigned)(size_t)(i))

/* DANG_GNUC_PRINTF(format_idx,arg_idx): Advise the compiler
 * that the arguments should be like printf(3); it may
 * optionally print type warnings.  */
#ifdef __GNUC__
#if     __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define DANG_GNUC_PRINTF( format_idx, arg_idx )    \
  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#endif
#endif
#ifndef DANG_GNUC_PRINTF                /* fallback: no compiler hint */
# define DANG_GNUC_PRINTF( format_idx, arg_idx )
#endif


#define DANG_NONZERO_IS_POWER_OF_TWO(value) \
  (((value) & ((value)-1)) == 0)
#define DANG_IS_POWER_OF_TWO(value) \
  ((value) != 0 && DANG_NONZERO_IS_POWER_OF_TWO (value))

void *dang_malloc   (size_t);
void *dang_malloc0  (size_t);
void  dang_free     (void *);
void *dang_realloc  (void *, size_t);
#define dang_new(type, count) ((type*)dang_malloc(sizeof(type) * (count)))
#define dang_newa(type, count) ((type*)dang_alloca(sizeof(type) * (count)))
#define dang_new0(type, count) ((type*)dang_malloc0(sizeof(type) * (count)))
char *dang_strdup   (const char *);
void *dang_memdup   (const void *, size_t len);
char *dang_strndup   (const char *, size_t len);
uint32_t dang_str_hash (const char *str);
uint32_t dang_util_binary_data_hash (size_t len, const uint8_t *data);
#define dang_assert(condition)   assert(condition)
#define dang_assert_not_reached() assert(0)
char *dang_strdup_printf (const char *format,
                          ...) DANG_GNUC_PRINTF(1,2);
char *dang_strdup_vprintf (const char *format,
                          va_list args);
char *dang_util_c_escape (unsigned len,
                          const void *data,
                          dang_boolean include_quotes);
char *dang_util_hex_escape (unsigned len,
                          const void *data);
/* writes 2*len hex chars to 'out' */
void  dang_util_hex_escape_inplace (char *out,
                                    unsigned len,
                                    const void *data);
void  dang_warning(const char *format,...) DANG_GNUC_PRINTF(1,2);
void  dang_warning(const char *format,...) DANG_GNUC_PRINTF(1,2);
void  dang_printerr(const char *format,...) DANG_GNUC_PRINTF(1,2);

typedef void (*DangDestroyNotify) (void *);


#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0
#define DANG_MIN(a,b)  ((a) < (b) ? (a) : (b))
#define DANG_MAX(a,b)  ((a) > (b) ? (a) : (b))


void dang_die (const char *format,
               ...) DANG_GNUC_PRINTF(1,2);
void dang_fatal_user_error (const char *format,
                            ...) DANG_GNUC_PRINTF(1,2);

char *dang_util_join_with_dot (unsigned  n_names,
                               char    **names);

/* free with dang_free() */
char **dang_util_split_by_dot (const char *dotted_name,
                               unsigned   *n_out);

/* Returns TRUE if all the bytes in mem are 0 */
dang_boolean dang_util_is_zero (const void *mem,
                                unsigned    len);

/* computes the product of a number of terms;
   useful for sizing tensors */
unsigned dang_util_uint_product (unsigned N, const unsigned *terms);

/* --- strings --- */
typedef struct _DangString DangString;
struct _DangString
{
  unsigned ref_count;
  unsigned len;
  char *str;
};
DangString *dang_string_new  (const char *str);
DangString *dang_string_new_len  (const char *str,
                              unsigned    len);
/*DangString *dang_string_new_printf (const char *str, ...) DANG_GNUC_PRINTF(1,2);*/
void        dang_string_unref(DangString *);
DangString *dang_string_ref  (DangString *);


/* for debugging, copy the string when debugging, ref-count otherwise */
DangString *dang_string_ref_copy  (DangString *);

DangString *dang_strings_concat (unsigned N,
                                 DangString **strs);
DangString *dang_string_joinv   (DangString *delim,
                                 unsigned N,
                                 DangString **strs);

/* this includes NUL-termination, but requires the
   user to fill-in the content of the string
   with valid utf8 */
DangString *dang_string_new_raw  (unsigned n_bytes);

DangString *dang_string_peek_boolean (dang_boolean b);

/* --- string-buffer --- */
typedef struct _DangStringBuffer DangStringBuffer;
struct _DangStringBuffer
{
  char *str;
  unsigned len;
  unsigned alloced;
};
#define DANG_STRING_BUFFER_INIT { NULL, 0, 0 }

/* WARNING: truncates the appended string! */
void dang_string_buffer_printf (DangStringBuffer *buf,
                                const char       *format,
                                ...) DANG_GNUC_PRINTF(2,3);
void dang_string_buffer_append (DangStringBuffer *buf,
                                const char       *str);
void dang_string_buffer_append_c(DangStringBuffer *buf,
                                char               c);
void dang_string_buffer_append_repeated_char
                                (DangStringBuffer *buf,
                                 char              c,
                                 unsigned          len);
void dang_string_buffer_append_len (DangStringBuffer *buf,
                                    const char       *str,
                                    unsigned          len);

/* --- arrays --- */
typedef struct _DangUtilArray DangUtilArray;
struct _DangUtilArray
{
  unsigned len;
  void *data;
  unsigned alloced;
  unsigned elt_size;
};
void dang_util_array_init       (DangUtilArray   *array,
                                 size_t           elt_size);
void dang_util_array_append     (DangUtilArray   *array,
                                 unsigned         count,
                                 const void      *data);
void dang_util_array_append_data(DangUtilArray   *array,
                                 unsigned         n_bytes,
                                 const void      *data);
void dang_util_array_remove     (DangUtilArray   *array,
                                 unsigned         start,
                                 unsigned         count);
void dang_util_array_insert     (DangUtilArray   *array,
                                 unsigned         n,
                                 const void      *data,
                                 unsigned         insert_pos);
void dang_util_array_set_size   (DangUtilArray   *array,
                                 unsigned          new_len);
void dang_util_array_set_size0  (DangUtilArray   *array,
                                 unsigned         new_len);
void dang_util_array_clear      (DangUtilArray   *array);

/* Initializations of global or stack-allocated arrays */
#define DANG_UTIL_ARRAY_STATIC_INIT(type) DANG_UTIL_ARRAY_STATIC_INIT_SIZEOF(sizeof(type))
#define DANG_UTIL_ARRAY_STATIC_INIT_SIZEOF(size) { 0, NULL, 0, (size) }

/* Function-style initialization of arrays */
#define DANG_UTIL_ARRAY_INIT(array, type) \
  dang_util_array_init (array, sizeof (type))

/* Standard indexing functions */
#define DANG_UTIL_ARRAY_INDEX(array, type, index) \
  ((type*)((array)->data))[index]
#define DANG_UTIL_ARRAY_INDEX_PTR(array, type, index) \
  (((type*)((array)->data)) + (index))


/* Binary-Data */
/* immutable byte arrays */
typedef struct _DangBinaryData DangBinaryData;
struct _DangBinaryData
{
  unsigned len;
  unsigned ref_count;
  /* data follows */
};
#if !DANG_DEBUG
#define DANG_BINARY_DATA_PEEK_DATA(binary_data) _DANG_BINARY_DATA_PEEK_DATA(binary_data)
#else
#define DANG_BINARY_DATA_PEEK_DATA(binary_data) dang_binary_data_peek_data(binary_data)
const uint8_t *dang_binary_data_peek_data (const DangBinaryData *);
#endif
DangBinaryData *dang_binary_data_new      (unsigned              len,
                                           const uint8_t *       data);
DangBinaryData *dang_binary_data_ref_copy (DangBinaryData       *binary_data);
DangBinaryData *dang_binary_data_ref      (DangBinaryData       *binary_data);
void            dang_binary_data_unref    (DangBinaryData       *binary_data);

/* --- errors --- */
typedef struct _DangError DangError;
struct _DangError
{
  unsigned ref_count;
  char *message;
  char *backtrace;
  DangString *filename;
  unsigned line_no;
};
DangError *dang_error_new        (const char *format,
                                  ...) DANG_GNUC_PRINTF(1,2);
void       dang_set_error        (DangError **error,
                                  const char *format,
                                  ...) DANG_GNUC_PRINTF(2,3);
void       dang_error_add_prefix (DangError  *error,
                                  const char *format,
                                  ...) DANG_GNUC_PRINTF(2,3);
void       dang_error_add_suffix (DangError  *error,
                                  const char *format,
                                  ...) DANG_GNUC_PRINTF(2,3);
void       dang_error_add_pos_suffix (DangError *error,
                                      DangCodePosition *cp);
DangError *dang_error_ref        (DangError  *error);
void       dang_error_unref      (DangError  *error);


/* --- DangStringTable --- */
typedef struct _DangStringTableNode DangStringTableNode;
struct _DangStringTableNode
{
  DangStringTableNode *parent, *left, *right;
  dang_boolean is_red;
  char *str;
};
void *dang_string_table_lookup (void *top_node,
                                const char *name);
void  dang_string_table_insert (void **top_node,
                                void  *tree_node,
                                void **conflict_out);

/* internals */
#define _DANG_BINARY_DATA_PEEK_DATA(binary_data) \
  (const uint8_t *) (((const DangBinaryData*)(binary_data)) + 1)
