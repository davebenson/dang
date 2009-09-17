
typedef struct _DangCompileContextVirtualMethodSlot DangCompileContextVirtualMethodSlot;

struct _DangCompileContext
{
  DangArray stubs;
  DangArray new_object_functions;
  dang_boolean finishing;
  DangCompileContextVirtualMethodSlot *virtual_method_slots;
};

DangCompileContext *dang_compile_context_new (void);
void                dang_compile_context_register(DangCompileContext *cc,
                                                  DangFunction       *stub);
dang_boolean        dang_compile_context_finish (DangCompileContext *cc,
                                                 DangError **error);
void dang_compile_context_free (DangCompileContext *cc);

/* 1: create a compile-context
 * 2: whenever dang_compile_function_invocation()
      uses a stub that isn't in a CC then 
      if (!dang_compile_function_invocation (function,builder,
                                   has_rv ? &return_val_result : NULL,
                                   n_args, compiled_params,
                                   &error)) */
