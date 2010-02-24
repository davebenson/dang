
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdarg.h>

typedef int dsk_boolean;
#define DSK_FALSE		0
#define DSK_TRUE		1

#define dsk_assert(x)  assert(x)

#define DSK_INLINE_FUNC inline

void *dsk_malloc (size_t);
void *dsk_malloc0 (size_t);
void  dsk_free (void *);
void *dsk_realloc (void *, size_t);
