
typedef struct _DskTableMergeBuffer DskTableMergeBuffer;
struct _DskTableMergeBuffer
{
  unsigned len;
  uint8_t *data;
  unsigned alloced;
};

typedef enum
{
  DSK_TABLE_MERGE_RETURN_A_FINAL,
  DSK_TABLE_MERGE_RETURN_A,
  DSK_TABLE_MERGE_RETURN_B_FINAL,
  DSK_TABLE_MERGE_RETURN_B,
  DSK_TABLE_MERGE_RETURN_BUFFER_FINAL,
  DSK_TABLE_MERGE_RETURN_BUFFER,
  DSK_TABLE_MERGE_DROP
} DskTableMergeResult;

typedef DskTableMergeResult (*DskTableMergeFunc) (unsigned key_len,
                                                  const uint8_t *key_data,
                                                  unsigned a_len,
                                                  const uint8_t *a_data,
                                                  unsigned b_len,
                                                  const uint8_t *b_data,
						  DskTableMergeBuffer *buffer,
						  dsk_boolean complete,
						  void *merge_data);
typedef int                 (*DskTableCompareFunc)(unsigned key_a_len,
                                                   const uint8_t *key_a_data,
                                                   unsigned key_b_len,
                                                   const uint8_t *key_b_data,
						   void *compare_data);
  
struct _DskTableConfig
{
  DskTableCompareFunc compare;
  void *compare_data;
  DskTableMergeFunc merge;
  void *merge_data;
  dsk_boolean chronological_lookup_merges;
  const char *dir;
  unsigned gzip_level;		/* 0 == identity */
};

DskTable *dsk_table_new (DskTableConfig *config,
                         DskError      **error);


