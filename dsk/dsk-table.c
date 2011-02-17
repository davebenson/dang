
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
  double actual_entry_count_ratio;
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
          ...

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
      g_set_error (error, SNAPDB_ERROR_DOMAIN_QUARK,
                   SNAPDB_ERROR_FILE_LOCK,
                   "error locking directory %s: %s",
                   dir, g_strerror (errno));
      close (fd);
      dsk_free (rv.dir);
      return FALSE;
    }
  rv.file_interface = options->file_interface;

  if (is_new)
    {
      /* create initial empty checkpoint */
      ...
    }
  else
    {
      /* open existing checkpoint / journal */
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
