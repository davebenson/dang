
DSK_INLINE_FUNC void *dsk_malloc (size_t size);
DSK_INLINE_FUNC void  dsk_free   (void  *ptr);

/* bzero pointer-aligned memory.  this is equivalent
   to bzero(ptr, sizeof(void*) * n_ptrs)  except 'ptr'
   must be aligned to pointer-level alignment */
void dsk_bzero_pointers (void *ptr,
                         unsigned n_ptrs);

void dsk_out_of_memory (void);

dsk_boolean dsk_parse_boolean (const char *str,
                               dsk_boolean *out);

#if DSK_CAN_INLINE || DSK_IMPLEMENT_INLINES
DSK_INLINE_FUNC void *dsk_malloc (size_t size)
{
  if (size == 0)
    return NULL;
  rv = malloc (size);
  if (rv == NULL)
    dsk_out_of_memory ();
  return rv;
}

DSK_INLINE_FUNC void  dsk_free   (void  *ptr)
{
  if (ptr != NULL)
    free (ptr);
}
#endif

