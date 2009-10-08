
/* Signatures are a way to figure out if
   a function is a match for a collection
   of arguments that we have.
 */

struct _DangSignature
{
  unsigned ref_count;
  unsigned n_params;
  DangFunctionParam *params;            /* name is optional in the parameters */
  DangValueType *return_type;
  dang_boolean is_templated;
  
  dang_boolean HACK_tracked;
};

DangSignature *dang_signature_new          (DangValueType     *return_type,
                                            unsigned           n_params,
                                            DangFunctionParam *params);     /* name is optional in the parameters */
DangSignature *dang_signature_ref          (DangSignature*);
void           dang_signature_unref        (DangSignature*);

/* peek at global void -> void signature */
DangSignature *dang_signature_void_func    (void);

/* matching against a signature */
typedef enum
{
  DANG_MATCH_QUERY_ELEMENT_SIMPLE_INPUT,
  DANG_MATCH_QUERY_ELEMENT_SIMPLE_OUTPUT,
  DANG_MATCH_QUERY_ELEMENT_FUNCTION_FAMILY,
  DANG_MATCH_QUERY_ELEMENT_UNTYPED_FUNCTION
} DangMatchQueryElementType;
const char *dang_match_query_element_type_name (DangMatchQueryElementType);
typedef struct 
{
  DangMatchQueryElementType type;
  union
  {
    DangValueType *simple_input;
    DangValueType *simple_output;
    DangFunctionFamily *function_family;
    DangUntypedFunction *untyped_function;
  } info;
} DangMatchQueryElement;
struct _DangMatchQuery
{
  unsigned n_elements;
  DangMatchQueryElement *elements;
  DangImports *imports;          /* to use by subqueries */
};

dang_boolean   dang_signature_test   (DangSignature *sig,
                                      DangMatchQuery *query);
dang_boolean   dang_function_params_test   (unsigned        n_params,
                                            DangFunctionParam *params,
                                            DangMatchQuery *query);

dang_boolean   dang_signature_test_templated(DangSignature *sig,
                                             DangMatchQuery *query,
                                            DangUtilArray     *tparam_type_pairs_out);

void dang_signature_dump (DangSignature *sig,
                          DangStringBuffer *buf);
void dang_match_query_dump (DangMatchQuery *mq,
                            DangStringBuffer *buf);
DangSignature * dang_signature_parse (DangExpr *args_expr,
                                      DangValueType *ret_type,
                                      DangError **error);


DangMatchQuery *dang_signature_make_match_query (DangSignature *);
#define dang_signature_match_query_free(q)   dang_free(q)



/* ignores parameter names */
dang_boolean dang_signatures_equal (DangSignature *a,
                                    DangSignature *b);
