
typedef struct _DangValueTypeTree DangValueTypeTree;
typedef struct _DangTreeNode DangTreeNode;
typedef struct _DangTree DangTree;

struct _DangValueTypeTree
{
  DangValueType base_type;
  DangValueType *key, *value;
  unsigned value_offset;
  //DangValueType *node_type;
  unsigned node_size;
  DangTreeNode *recycling_list;         /* connected by 'parent' */

  void (*destruct_tree_node) (DangValueTypeTree *, DangTreeNode *);
  DangTreeNode *(*copy_tree_node) (DangValueTypeTree *, DangTreeNode *, DangTreeNode *);
  DangValueIndexInfo index_info;

  /* for the tree of tree-types */
  DangValueTypeTree *parent,*left,*right;
  dang_boolean is_red;
};

struct _DangTreeNode
{
  unsigned is_red : 1;
  unsigned is_defunct : 1;
  unsigned visit_count : 30;
  DangTreeNode *parent, *left, *right;

  /* key and value follow */
};

struct _DangTree
{
  DangTreeNode *top;
  DangFunction *compare;		/* or NULL for default */
  unsigned size;
  DangThread *cached;                   /* for calling compare() efficiently */
};

DangValueType *dang_value_type_tree (DangValueType *key,
                                     DangValueType *value);

void           dang_tree_insert     (DangValueTypeTree *tree_type,
                                     DangTree          *tree,
                                     const void        *key,
                                     const void        *value);
