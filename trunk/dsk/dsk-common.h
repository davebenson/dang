
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>

typedef int dsk_boolean;
#define DSK_FALSE		0
#define DSK_TRUE		1

#define dsk_assert(x)  assert(x)
#define dsk_assert_not_reached()   \
  dsk_error("should not get here: %s:%u", __FILE__, __LINE__)
void dsk_error(const char *format, ...);
void dsk_warning(const char *format, ...);

#define DSK_CAN_INLINE  1
#define DSK_INLINE_FUNC static inline
#define _dsk_inline_assert(condition)  dsk_assert(condition)

void *dsk_malloc (size_t);
void *dsk_malloc0 (size_t);
void  dsk_free (void *);
void *dsk_realloc (void *, size_t);
char *dsk_strdup (const char *str);

void dsk_bzero_pointers (void *ptrs, unsigned n_ptrs);
