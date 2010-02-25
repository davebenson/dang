
typedef struct _DskMemPool DskMemPool;
typedef struct _DskMemPoolFixed DskMemPoolFixed;

/* --- Allocate-only Memory Pool --- */
struct _DskMemPool
{
  /*< private >*/
  void * all_chunk_list;
  char *chunk;
  unsigned chunk_left;
};

#define DSK_MEM_POOL_STATIC_INIT                        { NULL, NULL, 0 }


DSK_INLINE_FUNC void     dsk_mem_pool_init     (DskMemPool     *pool);
DSK_INLINE_FUNC void     dsk_mem_pool_init_buf (DskMemPool     *pool,
                                                void           *buffer,
                                                unsigned        buffer_size);
DSK_INLINE_FUNC void    *dsk_mem_pool_alloc    (DskMemPool     *pool,
                                                unsigned        size);
                void    *dsk_mem_pool_alloc0   (DskMemPool     *pool,
                                                unsigned        size);
DSK_INLINE_FUNC void    *dsk_mem_pool_alloc_unaligned(DskMemPool *pool,
                                                unsigned        size);
                char    *dsk_mem_pool_strdup   (DskMemPool      *pool,
                                                const char      *str);
DSK_INLINE_FUNC void     dsk_mem_pool_clear    (DskMemPool     *pool);

/* --- Allocate and free Memory Pool --- */
struct _DskMemPoolFixed
{
  /*< private >*/
  void * slab_list;
  char *chunk;
  unsigned pieces_left;
  unsigned piece_size;
  void * free_list;
};

#define DSK_MEM_POOL_FIXED_STATIC_INIT(size) \
                          { NULL, NULL, 0, size, NULL } 

DSK_INLINE_FUNC void     dsk_mem_pool_fixed_init_buf
                                                 (DskMemPoolFixed *pool,
                                                  unsigned          elt_size,
                                                  void *         buffer,
                                                  unsigned          buffer_n_elements);
void     dsk_mem_pool_fixed_init (DskMemPoolFixed  *pool,
                                       unsigned           size);
void * dsk_mem_pool_fixed_alloc     (DskMemPoolFixed  *pool);
void * dsk_mem_pool_fixed_alloc0    (DskMemPoolFixed  *pool);
void     dsk_mem_pool_fixed_free      (DskMemPoolFixed  *pool,
                                       void *          from_pool);
void     dsk_mem_pool_fixed_clear  (DskMemPoolFixed  *pool);



/* private */
void * dsk_mem_pool_must_alloc (DskMemPool *pool,
                                  unsigned     size);

/* ------------------------------*/
/* -- Inline Implementations --- */

#define _DSK_MEM_POOL_ALIGN(size)	\
  (((size) + sizeof(void *) - 1) / sizeof (void *) * sizeof (void *))
#define _DSK_MEM_POOL_SLAB_GET_NEXT_PTR(slab) \
  (* (void **) (slab))

#if defined(DSK_CAN_INLINE) || defined(DSK_IMPLEMENT_INLINES)
DSK_INLINE_FUNC void     dsk_mem_pool_init    (DskMemPool     *pool)
{
  pool->all_chunk_list = NULL;
  pool->chunk = NULL;
  pool->chunk_left = 0;
}
DSK_INLINE_FUNC void     dsk_mem_pool_init_buf   (DskMemPool     *pool,
                                                  void *        buffer,
                                                  unsigned         buffer_size)
{
  pool->all_chunk_list = NULL;
  pool->chunk = buffer;
  pool->chunk_left = buffer_size;
}

DSK_INLINE_FUNC void     dsk_mem_pool_align        (DskMemPool     *pool)
{
  unsigned mask = ((size_t) (pool->chunk)) & (sizeof(void *)-1);
  if (mask)
    {
      /* need to align chunk */
      unsigned align = sizeof (void *) - mask;
      pool->chunk_left -= align;
      pool->chunk = (char*)pool->chunk + align;
    }
}

DSK_INLINE_FUNC void * dsk_mem_pool_alloc_unaligned   (DskMemPool     *pool,
                                                       unsigned         size)
{
  char *rv;
  if (DSK_LIKELY (pool->chunk_left >= size))
    {
      rv = pool->chunk;
      pool->chunk_left -= size;
      pool->chunk = rv + size;
      return rv;
    }
  else
    /* fall through to non-inline version for
       slow malloc-using case */
    return dsk_mem_pool_must_alloc (pool, size);
}

DSK_INLINE_FUNC void * dsk_mem_pool_alloc            (DskMemPool     *pool,
                                                      unsigned         size)
{
  dsk_mem_pool_align (pool);
  return dsk_mem_pool_alloc_unaligned (pool, size);
}

DSK_INLINE_FUNC void     dsk_mem_pool_clear     (DskMemPool     *pool)
{
  void * slab = pool->all_chunk_list;
  while (slab)
    {
      void * new_slab = _DSK_MEM_POOL_SLAB_GET_NEXT_PTR (slab);
      g_free (slab);
      slab = new_slab;
    }
}
DSK_INLINE_FUNC void     dsk_mem_pool_fixed_init_buf
                                                 (DskMemPoolFixed *pool,
                                                  unsigned         elt_size,
                                                  void *           buffer,
                                                  unsigned         buffer_n_elements)
{
  pool->slab_list = NULL;
  pool->chunk = buffer;
  pool->pieces_left = buffer_n_elements;
  pool->piece_size = elt_size;
  pool->free_list = NULL;
}

#endif /* DSK_CAN_INLINE */

