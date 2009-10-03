
/* ---- Annotations --- */


typedef enum
{
  DANG_EXPR_ANNOTATION_TAG = 12345,
  DANG_EXPR_ANNOTATION_VAR_ID,
  DANG_EXPR_ANNOTATION_TENSOR_SIZES,
  DANG_EXPR_ANNOTATION_MEMBER,
  DANG_EXPR_ANNOTATION_INDEX_INFO
} DangExprAnnotationType;

#define DANG_EXPR_ANNOTATION_TYPE_IS_VALID(t) \
  (DANG_EXPR_ANNOTATION_TAG <= (t) && \
  (t) <= DANG_EXPR_ANNOTATION_VAR_ID)

struct _DangExprAnnotation
{
  /* keys of the table */
  DangExpr *expr;
  DangExprAnnotationType type;

  /* table infrastructure */
  DangExprAnnotation *left, *right, *parent;
  dang_boolean is_red;
  DangAnnotations *owner;
};

DangAnnotations *dang_annotations_new (void);

/* caution: assertion fails if we already have an annotation of that type+expr */
void   dang_expr_annotation_init(DangAnnotations *annotations,
                                 DangExpr        *expr,
                                 DangExprAnnotationType type,
                                 void            *annotation_to_init);
void * dang_expr_get_annotation (DangAnnotations *annotations,
                                 DangExpr        *expr,
                                 DangExprAnnotationType type);

void dang_annotations_free (DangAnnotations *);


typedef enum
{
  DANG_EXPR_TAG_VALUE,
  DANG_EXPR_TAG_NAMESPACE,
  DANG_EXPR_TAG_FUNCTION_FAMILY,
  DANG_EXPR_TAG_TYPE,
  DANG_EXPR_TAG_STATEMENT,              /* i.e. not a value */
  DANG_EXPR_TAG_UNTYPED_FUNCTION,
  DANG_EXPR_TAG_CLOSURE,
  DANG_EXPR_TAG_METHOD,
  DANG_EXPR_TAG_OBJECT_DEFINE
} DangExprTagType;
const char * dang_expr_tag_type_name (DangExprTagType type);

typedef struct _DangExprTag DangExprTag;
struct _DangExprTag
{
  DangExprAnnotation base;
  DangExprTagType tag_type;
  union
  {
    struct {
      DangValueType *type;
      dang_boolean is_lvalue;
      dang_boolean is_rvalue;
    } value;
    DangNamespace *ns;
    struct {
      DangFunctionFamily *family;
      DangFunction *function;
    } ff;
    DangValueType *type;
    DangUntypedFunction *untyped_function;
    struct {
      DangFunction *stub;
      unsigned n_closure_var_ids;
      DangVarId *closure_var_ids;
      DangSignature *sig;               /* signature of the value */
      DangValueType *function_type;     /* type of the function */
    } closure;
    struct {
      DangValueType *object_type;
      char *name;
      dang_boolean has_object;
      DangValueType *method_type;
      DangValueElement *method_element;  /* once resolved */
      unsigned index;           /* in method_element, once resolved */
    } method;
    struct {
      DangValueType *type;
    } object_define;
  } info;
};

typedef struct _DangExprVarId DangExprVarId;
struct _DangExprVarId
{
  DangExprAnnotation base;
  DangVarId var_id;
};

typedef struct _DangExprTensorSizes DangExprTensorSizes;
struct _DangExprTensorSizes
{
  DangExprAnnotation base;
  DangValueType *elt_type;
  unsigned rank;
  unsigned sizes[1];
};

typedef struct _DangExprMember DangExprMember;
struct _DangExprMember
{
  DangExprAnnotation base;
  dang_boolean dereference;
};

typedef struct _DangExprIndexInfo DangExprIndexInfo;
struct _DangExprIndexInfo
{
  DangExprAnnotation base;
  DangValueIndexInfo *index_info;
};

dang_boolean dang_expr_annotate_types (DangAnnotations *annotations,
                                       DangExpr    *expr,
                                       DangImports *imports,
				       DangVarTable *var_table,
				       DangError  **error);



