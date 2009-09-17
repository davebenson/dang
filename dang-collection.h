typedef struct _DangCollectionSpec DangCollectionSpec;

/* notes:
     the container must be on the stack or in an object.

     - if on the stack:
       - if the container isn't modified over
         the iteration, then use non-robust iterations
       - otherwise, use robust iteration
     - if in an object,
       - ensure the object itself isn't modified in the for loop
 */

struct _DangCollectionSpec
{
  unsigned n_params;
  DangValueType **params;
  DangValueType *iter_var_type;
  void (*compile_init_iter)    (DangCollectionSpec *collection,
                                DangCompileResult  *container,
			        DangVarId       iter_var);
  void (*compile_visit)        (DangCollectionSpec *collection,
                                DangCompileResult  *container,
			        DangVarId       iter_var,
			        DangVarId      *param_var_ids,
			        DangLabelId     label_if_done);
  void (*compile_robust_visit) (DangCollectionSpec *collection,
                                DangCompileResult  *container,
			        DangVarId       iter_var,
			        DangVarId      *param_var_ids,
			        DangLabelId     label_if_done);
};
                        
