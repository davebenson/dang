/* A trivial checkpoint class using mmap. */

#include "dsk.h"
#include "dsk-table-checkpoint.h"

typedef struct _TrivialTableCheckpoint TrivialTableCheckpoint;
struct _TrivialTableCheckpoint
{
  DskTableCheckpoint base;
  int fd;

  /* declared as volatile do writes cannot be reordered. */
  volatile char *mmapped;

  size_t mmapped_size;
  size_t cur_size;              /* always a multiple of 4;
                                   at 'cur_size' sits the 0xffffffff mark */
};

#define TRIVIAL_CP_MAGIC                0xde1414df

/* Format: header:
               magic            (TRIVIAL_CP_MAGIC as uint32le)
               version number   (should be 1, uint32le)
               cp_data size     (uint32le)
               cp_data          (binary-data)
               padding          (to multiple of 4 bytes)
           a list of
               key_length       (!= MAX_UINT32) (uint32le)
               value_length     (uint32le)
               key_data         (binary-data, no padding between key/value))
               value_data       (binary-data)
               0s               (padding to round to multiple of 4 bytes)
           0xff 0xff 0xff 0xff  (to mark the end-of-file)

   This format is designed so we can add atomically to
   the mmapped file by writing all members except the key-length,
   which will still be 0xffffffff, marking EOF until
   the last write of a uint32.  Hopefully, heh, this last write is atomic.
 */

#if (BYTE_ORDER == LITTLE_ENDIAN)
#  define UINT64_TO_LE(val)  (val)
#  define UINT32_TO_LE(val)  (val)
#elif (BYTE_ORDER == BIG_ENDIAN)
#  define UINT64_TO_LE(val)  bswap64(val)
#  define UINT32_TO_LE(val)  bswap32(val)
#else
#  error unknown endianness
#endif


static dsk_boolean
table_checkpoint_trivial__add  (DskTableCheckpoint *checkpoint,
                                unsigned            key_length,
                                const uint8_t      *key_data,
                                unsigned            value_length,
                                const uint8_t      *value_data,
                                DskError          **error)
{
  TrivialTableCheckpoint *cp = (TrivialTableCheckpoint *) checkpoint;
  unsigned to_add = 8           /* key/value lengths */
                  + (key_length+value_length+3)/4*4;  /*key/value + padding */
  if (cp->cur_size + to_add + 4 > cp->mmapped_size)
    {
      /* must grow mmap area */
      ...
    }

  /* write value length */
  * (uint32_t *) (cp->mmapped + cp->cur_size + 4) = UINT32_TO_LE (value_length);

  /* write key */
  memcpy (cp->mmapped + cp->cur_size + 8, key_data, key_length);

  /* write value */
  memcpy (cp->mmapped + cp->cur_size + 8 + key_length,
          value_data, value_length);

  if ((key_length + value_length) % 4 != 0)
    {
      /* zero padding */
      memset (cp->mmapped + cp->cur_size + 8 + key_length + value_length,
              0, 4 - (key_length + value_length) % 4);
    }

  /* write terminal 0xffffffff */
  * (uint32_t *) (cp->mmapped + cp->cur_size + to_add) = 0xffffffff;

  /* atomically add the entry to the journal. */
  * (uint32_t *) (cp->mmapped + cp->cur_size) = UINT32_TO_LE (key_length);

  /* update in-memory info for next add */
  cp->cur_size += to_add;
}

static dsk_boolean
table_checkpoint_trivial__sync (DskTableCheckpoint *checkpoint,
		                DskError          **error)
{
  /* no sync implementation yet */
  DSK_UNUSED (checkpoint);
  DSK_UNUSED (error);
  return DSK_TRUE;
}

static dsk_boolean
table_checkpoint_trivial__close(DskTableCheckpoint *checkpoint,
		                DskError          **error)
{
  TrivialTableCheckpoint *cp = (TrivialTableCheckpoint *) checkpoint;
  DSK_UNUSED (checkpoint);
  DSK_UNUSED (error);
  return DSK_TRUE;
}

static void
table_checkpoint_trivial__destroy(DskTableCheckpoint *checkpoint)
{
  if (munmap (cp->mmapped, cp->mmapped_size) < 0)
    dsk_warning ("error calling munmap(): %s", strerror (errno));
  close (cp->fd);
  dsk_free (cp);
}


static struct DskTableCheckpoint table_checkpoint_trivial__vfuncs =
{
  table_checkpoint_trivial__add,
  table_checkpoint_trivial__sync,
  table_checkpoint_trivial__close,
  table_checkpoint_trivial__destroy
};

static DskTableCheckpoint *
table_checkpoint_trivial__create (DskTableCheckpointInterface *iface,
                                  const char         *openat_dir,
                                  int                 openat_fd,
                                  const char         *basename,
                                  unsigned            cp_data_len,
                                  const uint8_t      *cp_data,
                                  DskTableCheckpoint *prior,  /* optional */
                                  DskError          **error)
{
  unsigned mmapped_size;
  
  if (prior == NULL)
    mmapped_size = DEFAULT_MMAP_SIZE;
  else
    {
      dsk_assert (prior->add == table_checkpoint_trivial__add);
      mmapped_size = ((TrivialTableCheckpoint *) prior)->mmapped_size;
    }

  if (mmapped_size < cp_data_len + 32)
    {
      ...
    }

  /* create fd */
  ...

  /* truncate / fallocate */
  ...

  /* mmap */
  ...

  rv = dsk_malloc (sizeof (TrivialTableCheckpoint));
  rv->base = table_checkpoint_trivial__vfuncs;
  rv->fd = fd;
  rv->mmapped = mmapped;
  rv->mmapped_size = mmapped_size;
  rv->cur_size = (12 + cp_data_len + 3) / 4 * 4;

  ((uint32_t *) (mmapped))[0] = UINT32_TO_LE (TRIVIAL_CP_MAGIC);
  ((uint32_t *) (mmapped))[1] = UINT32_TO_LE (1);  /* version */
  ((uint32_t *) (mmapped))[2] = UINT32_TO_LE (cp_data_len);  /* version */
  memcpy (mmapped + 12, cp_data, cp_data_len);
  if (cp_data_len % 4 != 0)
    memset (mmapped + 12 + cp_data_len, 0, 4 - cp_data_len % 4);
  * ((uint32_t *) (mmapped+rv->cur_size)) = 0xffffffff;        /* end-marker */

  return &rv->base;
}

static DskTableCheckpoint *
table_checkpoint_trivial__open   (DskTableCheckpointInterface *iface,
                                  const char         *openat_dir,
                                  int                 openat_fd,
                                  const char         *basename,
                                  unsigned           *cp_data_len_out,
                                  uint8_t           **cp_data_out,
                                  DskTableCheckpointReplayFunc func,
                                  void               *func_data,
                                  DskError          **error)
{
  /* open fd */
  ...

  /* fstat */
  ...

  /* mmap */
  ...

  /* replay */
  ...

  /* copy cp_data */
  ...
}

DskTableCheckpointInterface dsk_table_checkpoint_trivial =
{
  table_checkpoint_trivial__create,
  table_checkpoint_trivial__open
};
