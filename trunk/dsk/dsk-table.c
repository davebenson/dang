#include "dsk.h"
#include "dsk-table-checkpoint.h"

/* Checkpointing/journalling Algorithm:
   - start an "async-cp" checkpoint with the cp data,
     begin sync of all required files
   - add to journal as needed
   - when the sync finishes:
     - sync the checkpoint/journal file,
       move the checkpoint from async-cp to sync-cp,
       and sync the directory.
       (Delete any unused files)
     - if no or few entries were added to journal,
       wait for enough to flush the journal,
       then begin the sync process again.
     - if a lot of entries were added to the journal,
       flush immediately to a file and begin syncing again.
 */

typedef struct _DskTable DskTable;

struct _File
{
  uint64_t id;
  DskTableFileSeeker *seeker;           /* created on demand */

  /* These are the number of items, in the order they were received,
     ignoring merging. */
  uint64_t first_entry_index;
  uint64_t n_entries;

  /* Actual number of entries (post deletion and merging) */
  uint64_t entry_count;

  /* If this has an active merge job on it. */
  Merge *merge;

  /* If this doesn't have an active merge job,
     these are merge job sorted by ratio,
     then number of elements. */
  PossibleMerge *prev_merge;
  PossibleMerge *next_merge;

  File *prev, *next;
};

struct _Merge
{
  File *a, *b;
};

struct _PossibleMerge
{
  /* log2(a->entry_count / b->entry_count) * 1024 */
  int actual_entry_count_ratio_log2_b10;
  File *a, *b;
};


struct _DskTable
{
  DskTableCompareFunc compare;
  void *compare_data;
  DskTableMergeFunc merge;
  void *merge_data;
  dsk_boolean chronological_lookup_merges;
  char *dir;
  int dir_fd;
  DskTableFileInterface *file_interface;
};

DskTable   *dsk_table_new          (DskTableConfig *config,
                                    DskError      **error)
{
  DskTable rv;
  rv.compare = config->compare;
  rv.compare_data = config->compare_data;
  rv.merge = config->merge;
  rv.merge_data = config->merge_data;
  rv.chronological_lookup_merges = config->chronological_lookup_merges;
  rv.dir = dsk_strdup (config->dir);
  rv.dir_fd = open (rv.dir, O_RDONLY);
  if (rv.dir_fd < 0)
    {
      if (errno == ENOENT)
        {
          /* try making directory */
          if (!dsk_mkdir_recursive (rv.dir, 0777, error))
            return NULL;

          rv.dir_fd = open (rv.dir, O_RDONLY);
          if (rv.dir_fd < 0)
            goto opendir_failed;
          is_new = DSK_TRUE;
        }
      else
        goto opendir_failed;
    }
  dsk_fd_set_close_on_exec (rv.dir_fd);
  if (flock (rv.dir_fd, LOCK_EX|LOCK_NB) < 0)
    {
      dsk_set_error (error, "error locking directory %s: %s",
                     dir, g_strerror (errno));
      close (fd);
      dsk_free (rv.dir);
      return NULL;
    }
  rv.file_interface = options->file_interface;

  rv.cp_iface = options->checkpoint_interface;
  if (is_new)
    {
      /* create initial empty checkpoint */
      rv.cp = (*rv.cp_iface->create) (rv.cp_iface,
                                      fd, "ASYNC-CP",
                                      0, NULL, error);
    }
  else if (was shutdown unexpectedly)
    {
      /* open existing checkpoint / journal */
      ...
    }
  else
    {
      /* use ASYNC-CP if available, SYNC-CP if available, or die. */
      ...
    }

  return dsk_memdup (sizeof (rv), &rv);

opendir_failed:
  dsk_set_error (error, "error opening directory %s for locking: %s",
                 rv.dir, strerror (errno));
  dsk_free (rv.dir);
  return NULL;
}

dsk_boolean
dsk_table_lookup       (DskTable       *table,
                        unsigned        key_len,
                        const uint8_t  *key_data,
                        unsigned       *value_len_out,
                        const uint8_t **value_data_out,
                        DskError      **error)
{
  ...
}

dsk_boolean
dsk_table_insert       (DskTable       *table,
                        unsigned        key_len,
                        const uint8_t  *key_data,
                        unsigned        value_len,
                        const uint8_t  *value_data,
                        DskError      **error)
{
  ...
}

void        dsk_table_destroy      (DskTable       *table)
{
  ...
}

void        dsk_table_destroy_erase(DskTable       *table)
{
  ...
}
