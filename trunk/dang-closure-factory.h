/* TODO: closures and functions generally should have the ability to call
 * a function utilizing a piece of their own stack. */

DangClosureFactory *dang_closure_factory_new  (DangSignature *underlying_sig,
                                               unsigned       n_params_to_curry);
DangClosureFactory *dang_closure_factory_ref  (DangClosureFactory*);
void                dang_closure_factory_unref(DangClosureFactory*);

unsigned dang_closure_factory_get_n_inputs (DangClosureFactory *factory);
DangFunction       *dang_function_new_closure (DangClosureFactory *factory,
                                               DangFunction       *underlying,
                                               void              **param_values);

void _dang_closure_factory_destruct_closure_data (DangClosureFactory *factory,
                                                  void *function);

void _dang_closure_factory_debug_init (void);

