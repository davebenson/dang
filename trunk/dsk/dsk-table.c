#include "dsk.h"
#include "dsk-table-checkpoint.h"
#include "../gskrbtreemacros.h"
#include "../gsklistmacros.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <sys/file.h>
#include "dsk-table-helper.h"

#define DSK_TABLE_CPDATA_MAGIC   0xf423514e

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

#define MAX_ID_LENGTH     20
#define FILE_BASENAME_FORMAT   "%llx"

typedef struct _Merge Merge;
typedef struct _PossibleMerge PossibleMerge;
typedef struct _File File;
typedef struct _TreeNode TreeNode;

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
  uint64_t out_id;
  DskTableFileWriter *out;
  uint64_t entries_written;
  uint64_t inputs_remaining;

  Merge *next;
};

struct _PossibleMerge
{
  /* log2(a->entry_count / b->entry_count) * 1024 */
  int actual_entry_count_ratio_log2_b10;
  File *a, *b;

  /* possible merges, ordered by actual_entry_count_ratio_log2_b10,
     then a->n_entries then a->first_entry_index */
  dsk_boolean is_red;
  PossibleMerge *left, *right, *parent;
};

struct _TreeNode
{
  unsigned key_length;
  unsigned value_length;
  unsigned value_alloced;
  TreeNode *left, *right, *parent;
  dsk_boolean is_red;
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
  DskTableCheckpointInterface *cp_interface;
  DskTableCheckpoint *cp;

  DskTableBuffer merge_buffers[2];
  uint64_t next_id;

  /* All existing files, sorted chronologically */
  File *oldest_file, *newest_file;

  /* Merge jobs that have begun, sorted by the number of
     inputs remaining to process.  (a proxy for "time remaining") */
  Merge *running_merges;
  unsigned n_merge_jobs;
  unsigned max_merge_jobs;

  /* small in-memory tree for fast lookups on the most recent values */
  TreeNode *small_tree;

  PossibleMerge *possible_merge_tree;

  char basename[MAX_ID_LENGTH];
};
/* NOTE NOTE: GET_SMALL_TREE() uses the local variable "table"!!! */
#define COMPARE_SMALL_TREE_NODES(a,b, rv) \
  rv = table->compare (a->key_length, (const uint8_t*)(a+1), \
                       b->key_length, (const uint8_t*)(b+1), \
                       table->compare_data)
#define GET_IS_RED(a)   (a)->is_red
#define SET_IS_RED(a,v) (a)->is_red = (v)
#define GET_SMALL_TREE() \
  (table)->small_tree, TreeNode *, GET_IS_RED, SET_IS_RED, \
  parent, left, right, COMPARE_SMALL_TREE_NODES

#define COMPARE_POSSIBLE_MERGES(A,B, rv) \
  rv = (A->actual_entry_count_ratio_log2_b10 < B->actual_entry_count_ratio_log2_b10) ? -1 \
     : (A->actual_entry_count_ratio_log2_b10 > B->actual_entry_count_ratio_log2_b10) ? +1 \
     : (A->a->n_entries < B->a->n_entries) ? -1 \
     : (A->a->n_entries > B->a->n_entries) ? +1 \
     : (A->a->first_entry_index < B->a->first_entry_index) ? -1 \
     : (A->a->first_entry_index > B->a->first_entry_index) ? +1 \
     : 0
#define GET_POSSIBLE_MERGE_TREE() \
  (table)->possible_merge_tree, PossibleMerge *, GET_IS_RED, SET_IS_RED, \
  parent, left, right, COMPARE_POSSIBLE_MERGES


#define GET_FILE_LIST() \
  File *, table->oldest_file, table->newest_file, prev, next

static inline void
set_table_file_basename (DskTable *table,
                         uint64_t  id)
{
  snprintf (table->basename, MAX_ID_LENGTH, FILE_BASENAME_FORMAT, id);
}

static TreeNode *
lookup_tree_node (DskTable           *table,
                  unsigned            key_length,
                  const uint8_t      *key_data)
{
  TreeNode *result;
#define COMPARE_KEY_TO_NODE(a,b, rv)           \
  rv = table->compare (key_length,             \
                       key_data,               \
                       b->key_length,          \
                       (const uint8_t*)(b+1),  \
                       table->compare_data)
  GSK_RBTREE_LOOKUP_COMPARATOR (GET_SMALL_TREE (), unused,
                                COMPARE_KEY_TO_NODE, result);
#undef COMPARE_KEY_TO_NODE
  return result;
}

static void
set_node_value (DskTable      *table,
                TreeNode      *node,
                unsigned       length,
                const uint8_t *data)
{
  if (node->value_alloced < length)
    {
      unsigned value_alloced = length + 16 - node->key_length % 8;
      TreeNode *new_node = dsk_malloc (sizeof (TreeNode)
                                       + node->key_length + value_alloced);
      new_node->key_length = node->key_length;
      new_node->value_alloced = value_alloced;
      memcpy (new_node + 1, node + 1, node->key_length);
      GSK_RBTREE_REPLACE_NODE (GET_SMALL_TREE (), node, new_node);
      dsk_free (node);
      node = new_node;
    }
  memcpy ((uint8_t*)(node + 1) + node->key_length, data, length);
  node->value_length = length;
}

static dsk_boolean
handle_checkpoint_replay_element (unsigned            key_length,
                                  const uint8_t      *key_data,
                                  unsigned            value_length,
                                  const uint8_t      *value_data,
                                  void               *replay_data,
                                  DskError          **error)
{
  DskTable *table = replay_data;
  TreeNode *node = lookup_tree_node (table, key_length, key_data);

  (void) error;

  if (node == NULL)
    {
      unsigned value_alloced = value_length + 16 - key_length % 8;
      TreeNode *node = dsk_malloc (sizeof (TreeNode) + key_length + value_alloced);
      TreeNode *conflict;
      memcpy (node + 1, key_data, key_length);
      memcpy ((uint8_t*)(node+1) + key_length, value_data, value_length);
      node->key_length = key_length;
      node->value_length = value_length;
      node->value_alloced = value_alloced;
      GSK_RBTREE_INSERT (GET_SMALL_TREE (), node, conflict);
      dsk_assert (conflict == NULL);
    }
  else
    {
      /* perform merge */
      switch (table->merge (key_length, key_data,
                            node->value_length,
                            (uint8_t *) (node+1) + node->key_length,
                            value_length, value_data,
                            &table->merge_buffers[0],
                            DSK_FALSE,
                            table->merge_data))
        {
        case DSK_TABLE_MERGE_RETURN_A_FINAL:
        case DSK_TABLE_MERGE_RETURN_A:
          break;

        case DSK_TABLE_MERGE_RETURN_B_FINAL:
        case DSK_TABLE_MERGE_RETURN_B:
          /* NOTE: may delete 'node' */
          set_node_value (table, node, value_length, value_data);
          break;

        case DSK_TABLE_MERGE_RETURN_BUFFER_FINAL:
        case DSK_TABLE_MERGE_RETURN_BUFFER:
          /* NOTE: may delete 'node' */
          set_node_value (table, node,
                          table->merge_buffers[0].length,
                          table->merge_buffers[0].data);
          break;

        case DSK_TABLE_MERGE_DROP:
          GSK_RBTREE_REMOVE (GET_SMALL_TREE (), node);
          dsk_free (node);
          break;
        }
    }
  return DSK_TRUE;
}

static void
create_possible_merge (DskTable *table,
                       File     *a)
{
  File *b = a->next;
  PossibleMerge *pm;
  PossibleMerge *conflict;
  int lg2_10;
  dsk_assert (b != NULL);
  if (a->entry_count == 0 && b->entry_count == 0)
    {
      lg2_10 = 0;
    }
  else if (a->entry_count == 0)
    {
      lg2_10 = INT_MIN;
    }
  else if (b->entry_count == 0)
    {
      lg2_10 = INT_MAX;
    }
  else
    {
      double ratio = (double)a->entry_count / (double)b->entry_count;
      double lg_ratio = log (ratio) * M_LN2 * 1024;
      lg2_10 = (int) lg_ratio;
    }
  pm = dsk_malloc (sizeof (PossibleMerge));
  pm->actual_entry_count_ratio_log2_b10 = lg2_10;
  pm->a = a;
  pm->b = b;
  GSK_RBTREE_INSERT (GET_POSSIBLE_MERGE_TREE (), pm, conflict);
}

static void
free_small_tree_recursive (TreeNode *node)
{
  if (node->left != NULL)
    free_small_tree_recursive (node->left);
  if (node->right != NULL)
    free_small_tree_recursive (node->right);
  dsk_free (node);
}

static uint32_t
parse_uint32_le (const uint8_t *data)
{
#if BYTE_ORDER == LITTLE_ENDIAN
  uint32_t v;
  memcpy (&v, data, 4);
  return v;
#else
  return (((uint32_t) data[0]))
       | (((uint32_t) data[1]) << 8)
       | (((uint32_t) data[2]) << 16)
       | (((uint32_t) data[3]) << 24);
#endif
}
static uint64_t
parse_uint64_le (const uint8_t *data)
{
#if BYTE_ORDER == LITTLE_ENDIAN
  uint64_t v;
  memcpy (&v, data, 8);
  return v;
#else
  uint32_t lo = parse_uint32_le (data);
  uint32_t hi = parse_uint32_le (data+4);
  return ((uint64_t)lo) | (((uint64_t)hi) << 32);
#endif
}

static dsk_boolean
parse_checkpoint_data (DskTable *table,
                       unsigned  len,
                       const uint8_t *data,
                       DskError  **error)
{
  uint32_t magic, version;
  unsigned i, n_files;
  if (len < 8)
    {
      dsk_set_error (error, "checkpoint too small (must be 8 bytes, got %u)",
                     len);
      return DSK_FALSE;
    }
  magic = parse_uint32_le (data + 0);
  version = parse_uint32_le (data + 4);
  if (magic != DSK_TABLE_CPDATA_MAGIC)
    {
      dsk_set_error (error, "checkpoint data magic invalid");
      return DSK_FALSE;
    }
  if (version != 0)
    {
      dsk_set_error (error, "bad version of checkpoint data");
      return DSK_FALSE;
    }

  if ((len - 8) % 32 != 0)
    {
      dsk_set_error (error, "checkpoint data should be a multiple of 32 bytes");
      return DSK_FALSE;
    }
  n_files = (len - 8) / 32;
  for (i = 0; i < n_files; i++)
    {
      File *file = dsk_malloc (sizeof (File));
      const uint8_t *info = data + 8 + 32 * i;
      file->id = parse_uint64_le (info + 0);
      file->seeker = NULL;
      file->first_entry_index = parse_uint64_le (info + 8);
      file->n_entries = parse_uint64_le (info + 16);
      file->entry_count = parse_uint64_le (info + 24);
      file->merge = NULL;
      file->prev_merge = NULL;
      file->next_merge = NULL;
      GSK_LIST_APPEND (GET_FILE_LIST (), file);
      if (file->id >= table->next_id)
        table->next_id = file->id + 1;
    }
  return DSK_TRUE;
}

static void
kill_possible_merge (DskTable *table,
                     PossibleMerge *possible)
{
  dsk_assert (possible->a->next_merge == possible);
  dsk_assert (possible->b->prev_merge == possible);
  GSK_RBTREE_REMOVE (GET_POSSIBLE_MERGE_TREE (), possible);
  possible->a->next_merge = possible->b->prev_merge = NULL;
  dsk_free (possible);
}

static Merge *
start_merge_job (DskTable *table,
                 PossibleMerge *possible,
                 DskError **error)
{
  Merge *merge;
  Merge **p_next;

  if (possible->a->prev_merge)
    kill_possible_merge (table, possible->a->prev_merge);
  if (possible->b->next_merge)
    kill_possible_merge (table, possible->b->next_merge);

  merge = dsk_malloc (sizeof (Merge));
  merge->a = possible->a;
  merge->b = possible->b;
  merge->out_id = table->next_id++;
  set_table_file_basename (table, merge->out_id);
  merge->out = table->file_interface->new_writer (table->file_interface,
                                                  table->dir, table->dir_fd,
                                                  table->basename,
                                                  error);
  merge->entries_written = 0;
  merge->inputs_remaining = merge->a->entry_count + merge->b->entry_count;

  /* insert sorted */
  p_next = &table->running_merges;
  while (*p_next != NULL
      && merge->inputs_remaining > (*p_next)->inputs_remaining)
    p_next = &((*p_next)->next);
  merge->next = *p_next;
  *p_next = merge;
  table->n_merge_jobs += 1;

  /* set files' 'merge' members */
  possible->a->merge = possible->b->merge = merge;

  kill_possible_merge (table, possible);
  return merge;
}

static dsk_boolean
maybe_start_merge_jobs (DskTable *table,
                        DskError **error)
{
  PossibleMerge *best;
  while (table->n_merge_jobs < table->max_merge_jobs)
    {
      GSK_RBTREE_FIRST (GET_POSSIBLE_MERGE_TREE (), best);
      if (best == NULL)
        return DSK_TRUE;

      ...

      if (start_merge_job (table, best, error) == NULL)
        return DSK_FALSE;
    }
  return DSK_TRUE;
}

DskTable   *dsk_table_new          (DskTableConfig *config,
                                    DskError      **error)
{
  DskTable rv;
  dsk_boolean is_new = DSK_FALSE;
  memset (&rv, 0, sizeof (rv));
  rv.compare = config->compare;
  rv.compare_data = config->compare_data;
  rv.merge = config->merge;
  rv.merge_data = config->merge_data;
  rv.chronological_lookup_merges = config->chronological_lookup_merges;
  rv.dir = dsk_strdup (config->dir);
  rv.dir_fd = open (rv.dir, O_RDONLY);
  rv.max_merge_jobs = 16;
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
                     rv.dir, strerror (errno));
      close (rv.dir_fd);
      dsk_free (rv.dir);
      return NULL;
    }
  rv.file_interface = config->file_interface;

  rv.cp_interface = config->cp_interface;
  if (is_new)
    {
      /* create initial empty checkpoint */
      rv.cp = (*rv.cp_interface->create) (rv.cp_interface,
                                          rv.dir, rv.dir_fd, "ASYNC-CP",
                                          0, NULL, error);
      rv.next_id = 1;
    }
  else
    {
      {
        int alive_fd = dsk_table_helper_openat (rv.dir, rv.dir_fd,
                                                "ALIVE", "",
                                                O_RDONLY, 0, NULL);
        if (alive_fd >= 0)
          {
            dsk_warning ("table was unexpectedly shutdown");
            close (alive_fd);
            alive_fd = -1;
          }
      }
      unsigned cp_data_len;
      uint8_t *cp_data;

      rv.cp = (*rv.cp_interface->open) (rv.cp_interface,
                                        rv.dir, rv.dir_fd,
                                        "ASYNC-CP",
                                        &cp_data_len, &cp_data,
                                        handle_checkpoint_replay_element, &rv,
                                        error);
      if (rv.cp == NULL)
        goto cp_open_failed;

      /* open files. */
      if (cp_data_len != 0)
        {
          if (!parse_checkpoint_data (&rv, cp_data_len, cp_data, error))
            {
              goto cp_open_failed;
            }
        }

      /* create possible merge jobs */
      if (rv.oldest_file != NULL)
        {
          File *file;
          for (file = rv.oldest_file; file->next != NULL; file = file->next)
            create_possible_merge (&rv, file);
        }
    }

  /* Stuff which requires having the real Table pointer */
  {
    DskTable *table;
    table = dsk_memdup (sizeof (rv), &rv);
    maybe_start_merge_jobs (table);
    return table;
  }

opendir_failed:
  dsk_set_error (error, "error opening directory %s for locking: %s",
                 rv.dir, strerror (errno));
  dsk_free (rv.dir);
  return NULL;

cp_open_failed:
  close (rv.dir_fd);
  dsk_free (rv.dir);
  if (rv.small_tree != NULL)
    free_small_tree_recursive (rv.small_tree);
  return NULL;
}


/* *res_index_inout is -1 if no result has been located yet.
   it may be 0 or 1 depending on which merge_buffer has the result.
 */
static dsk_boolean
do_file_lookup (DskTable    *table,
                File        *file,
                unsigned     key_length,
                const uint8_t *key_data,
                int         *res_index_inout,
                dsk_boolean *is_done_out,
                dsk_boolean  is_final,
                DskError   **error)
{
  DskError *e = NULL;
  unsigned value_len;
  const uint8_t *value_data;
  if (file->seeker == NULL)
    {
      DskTableFileInterface *iface = table->file_interface;
      set_table_file_basename (table, file->id);
      file->seeker = iface->new_seeker (iface,
                                        table->dir, table->dir_fd,
                                        table->basename,
                                        error);
      if (file->seeker == NULL)
        return DSK_FALSE;
    }
  if (!seeker->find (seeker, seeker_find_function, table,
                     DSK_TABLE_FILE_FIND_ANY,
                     NULL, NULL, &value_len, &value_data,
                     &e))
    {
      if (e)
        {
          if (error)
            *error = e;
          else
            dsk_error_unref (e);
          return DSK_FALSE;
        }
      return DSK_TRUE;
    }

  if (*res_index_inout < 0)
    {
      *res_index_inout = 0;
      memcpy (dsk_table_buffer_set_size (&table->merge_buffers[0],
                                         value_len),
              value_data,
              value_len);
    }
  else
    {
      unsigned a_len = value_len;
      const uint8_t *a_data = value_data;
      DskTableMergeBuffer *res = &table->merge_buffers[*res_index_inout];
      unsigned b_len = res->length;
      const uint8_t *b_data = res->data;
      dsk_boolean is_correct;
      if (table->chronological_lookup_merges)
        {
          { unsigned swap = a_len; a_len = b_len; b_len = swap; }
          { const uint8_t *swap = a_data; a_data = b_data; b_data = swap; }
        }
      switch (table->merge (key_length, key_data,
                            a_len, a_data,
                            b_len, b_data,
                            &table->merge_buffers[1 - *res_index_inout],
                            is_final,
                            table->merge_data))
        {
        case DSK_TABLE_MERGE_RETURN_A_FINAL:
          is_correct = a_data == res->data;
          break;
        case DSK_TABLE_MERGE_RETURN_A:
          is_correct = a_data == res->data;
          *is_done_out = DSK_TRUE;
          break;
        case DSK_TABLE_MERGE_RETURN_B_FINAL:
          is_correct = b_data == res->data;
          break;
        case DSK_TABLE_MERGE_RETURN_B:
          is_correct = b_data == res->data;
          *is_done_out = DSK_TRUE;
          break;
        case DSK_TABLE_MERGE_RETURN_BUFFER_FINAL:
          *res_index_inout = 1 - *res_index_inout;
          *is_done_out = DSK_TRUE;
          return DSK_TRUE;
        case DSK_TABLE_MERGE_RETURN_BUFFER:
          *res_index_inout = 1 - *res_index_inout;
          return DSK_TRUE;
        case DSK_TABLE_MERGE_DROP:
          *res_index_inout = -1;
          return DSK_TRUE;
        }
      if (!is_correct)
        {
          memcpy (dsk_table_buffer_set_size (res, value_len),
                  value_data, value_len);
        }
    }
  return DSK_TRUE;
}

dsk_boolean
dsk_table_lookup       (DskTable       *table,
                        unsigned        key_len,
                        const uint8_t  *key_data,
                        unsigned       *value_len_out,
                        const uint8_t **value_data_out,
                        DskError      **error)
{
  int res_index = -1;
  dsk_boolean is_done = DSK_FALSE;
  if (table->chronological_lookup_merges)
    {
      /* iterate files from oldest to newest */
      File *file = table->oldest_file;
      while (!is_done && file != NULL)
        {
          if (!do_file_lookup (table, file, &res_index, &is_done, DSK_FALSE, error))
            return DSK_FALSE;
          file = file->next;
        }
      if (!is_done)
        do_tree_lookup (table, &res_index, &is_done, error);
    }
  else
    {
      File *file = table->newest_file;
      do_tree_lookup (table, &res_index, &is_done, error);
      while (!is_done && file != NULL)
        {
          if (!do_file_lookup (table, file, &res_index, &is_done, file->prev == NULL, error))
            return DSK_FALSE;
          file = file->prev;
        }
    }
  if (res_index < 0)
    return DSK_FALSE;
  if (value_len_out != NULL)
    *value_len_out = table->merge_buffers[res_index].length;
  if (value_data_out != NULL)
    *value_data_out = table->merge_buffers[res_index].data;
  return DSK_TRUE;
}

dsk_boolean
dsk_table_insert       (DskTable       *table,
                        unsigned        key_len,
                        const uint8_t  *key_data,
                        unsigned        value_len,
                        const uint8_t  *value_data,
                        DskError      **error)
{
  /* add to tree */
  add_to_tree (table, key_len, key_data, value_length, value_data);

  /* add to checkpoint */
  ...

  /* maybe flush tree, creating new checkpoint */
  if (table->cp_n_entries == table->flush_period)
    {
      /* write tree to disk */
      ...

      /* create new checkpoint */
      ...

      /* destroy old checkpoint */
      ...

      /* move new checkpoint into place (atomic) */
      ...

      table->cp_n_entries = 0;
    }

  /* work on any running merge jobs */
  n_processed = 0;
  while (n_processed < 512 && table->file != NULL)
    {
      if (!run_first_merge_job (table, error))
        return DSK_FALSE
      n_processed++;
    }

  /* start more jobs */
  maybe_start_merge_jobs (table);

  /* do more work, if there's something to do */
  while (n_processed < 512 && table->file != NULL)
    {
      if (!run_first_merge_job (table, error))
        return DSK_FALSE
      n_processed++;
    }
  return DSK_TRUE;
}

void        dsk_table_destroy      (DskTable       *table)
{
  ...
}

void        dsk_table_destroy_erase(DskTable       *table)
{
  ...
}
