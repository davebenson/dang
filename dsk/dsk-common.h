
/* true configuration */
#define DSK_HAVE_IPV6  1


#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>

typedef int dsk_boolean;
#define DSK_FALSE		0
#define DSK_TRUE		1


/* branch-hinting macros */
#define DSK_LIKELY(condition)     (condition)
#define DSK_UNLIKELY(condition)   (condition)
#define DSK_UNUSED(var)           ((void)(var))

/* Seconds since 1970 GMT (aka the epoch).
   Note that many platforms define time_t as a 32-bit quantity--
   it is always 64-bit in dsk. */
typedef int64_t dsk_time_t;
dsk_time_t dsk_get_current_time ();

typedef void (*DskDestroyNotify) (void *data);

typedef enum
{
  DSK_IO_RESULT_SUCCESS,
  DSK_IO_RESULT_AGAIN,
  DSK_IO_RESULT_EOF,            /* only for read operations */
  DSK_IO_RESULT_ERROR
} DskIOResult;

#define dsk_assert(x)  assert(x)
#define dsk_assert_not_reached()   \
  dsk_error("should not get here: %s:%u", __FILE__, __LINE__)

#define DSK_CAN_INLINE  1
#define DSK_INLINE_FUNC static inline
#define _dsk_inline_assert(condition)  dsk_assert(condition)

/* DSK_GNUC_PRINTF(format_idx,arg_idx): Advise the compiler
 * that the arguments should be like printf(3); it may
 * optionally print type warnings.  */
#ifdef __GNUC__
#if     __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define DSK_GNUC_PRINTF( format_idx, arg_idx )    \
  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#endif
#endif
#ifndef DSK_GNUC_PRINTF                /* fallback: no compiler hint */
# define DSK_GNUC_PRINTF( format_idx, arg_idx )
#endif

/* logging */
/* fatal user level error:  exit with status 1 */
void dsk_error(const char *format, ...) DSK_GNUC_PRINTF(1,2);

/* non-fatal warning message */
void dsk_warning(const char *format, ...) DSK_GNUC_PRINTF(1,2);

/* programmer error:  only needed during development, hopefully.. */
void dsk_die(const char *format, ...) DSK_GNUC_PRINTF(1,2);


/* allocators (and utilities) which behave fatally */
void *dsk_malloc (size_t);
void *dsk_malloc0 (size_t);
void  dsk_free (void *);
void *dsk_realloc (void *, size_t);
char *dsk_strdup (const char *str);
void *dsk_memdup (size_t, const void *);

/* utility for initializing objects */
void dsk_bzero_pointers (void *ptrs, unsigned n_ptrs);


dsk_boolean dsk_parse_boolean (const char *str, dsk_boolean *out);
