
typedef struct _DangValueTypeTree DangValueTypeTree;
typedef struct _DangTreeNode DangTreeNode;
typedef struct _DangConstantTree DangConstantTree;
typedef struct _DangTree DangTree;

typedef struct _DangValueTreeTypes DangValueTreeTypes;
struct _DangValueTypeTree
{
  DangValueType base_type;
  DangValueTreeTypes *owner;
  DangValueIndexInfo index_info;
};
struct _DangValueTreeTypes
{
  DangValueType *key, *value;
  unsigned value_offset;
  //DangValueType *node_type;
  unsigned node_size;
  DangTreeNode *recycling_list;         /* connected by 'parent' */

  void (*destruct_tree_node) (DangValueTreeTypes *, DangTreeNode *);
  DangTreeNode *(*copy_tree_node) (DangValueTreeTypes *, DangTreeNode *, DangTreeNode *);
  DangValueIndexInfo index_info;

  /* for the tree of tree-types */
  DangValueTreeTypes *parent,*left,*right;
  dang_boolean is_red;

  DangValueTypeTree types[2];           /* 0=mutable, 1=constant */
};


struct _DangTreeNode
{
  unsigned is_red : 1;
  unsigned is_defunct : 1;
  unsigned visit_count : 30;
  DangTreeNode *parent, *left, *right;

  /* key and value follow */
};

struct _DangConstantTree
{
  unsigned ref_count;
  DangTreeNode *top;
  DangFunction *compare;		/* or NULL for default */
  unsigned size;
};

struct _DangTree
{
  DangConstantTree *v;
  unsigned ref_count;
};

DangValueType *dang_value_type_constant_tree (DangValueType *key,
                                     DangValueType *value);
DangValueType *dang_value_type_tree (DangValueType *key,
                                     DangValueType *value);

void           dang_tree_insert     (DangValueTypeTree *tree_type,
                                     DangTree          *tree,
                                     const void        *key,
                                     const void        *value);
