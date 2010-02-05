#include "dsk.h"

void dsk_bzero_pointers (void *ptr,
                         unsigned n_ptrs)
{
  void **p = ptr;
  while (n_ptrs--)
    *p++ = NULL;
}

