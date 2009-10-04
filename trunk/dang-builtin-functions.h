

/* NOTE: all tensor types must be of the same rank; n_tensor_args >= 1 */
DangFunction *dang_builtin_function_map_tensors (unsigned n_tensor_args,
					         DangValueType **tensor_types,
                                                 DangValueType *output_tensor_type);
