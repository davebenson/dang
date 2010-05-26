// EXPERIMENT: vararg-free printing

/* DskPrint *codegen = dsk_print_new_fp (stdout, NULL);
   dsk_print_set_string (codegen, "key", "value");
   dsk_print (codegen, "this is $key!\n");
   dsk_print_context_free (codegen);
 */

typedef dsk_boolean (*DskPrintAppendFunc) (unsigned   length,
                                           void      *data,
					   DskError **error);

typedef struct _DskPrint DskPrint;

DskPrint *dsk_print_new    (DskPrintAppendFunc append,
                            void              *data,
			    DskDestroyNotify   destroy);
void      dsk_print_free   (DskPrint *print);

/* stdio.h support */
DskPrint *dsk_print_new_fp (void *file_pointer);
DskPrint *dsk_print_new_fp_fclose (void *file_pointer);
DskPrint *dsk_print_new_fp_pclose (void *file_pointer);

/* Setting variables for interpolation */
void dsk_print_set_string          (DskPrint    *context,
                                    const char  *variable_name,
			            const char  *value);
void dsk_print_set_int             (DskPrint    *context,
                                    const char  *variable_name,
			            int          value);
void dsk_print_set_uint            (DskPrint    *context,
                                    const char  *variable_name,
			            unsigned     value);
void dsk_print_set_int64           (DskPrint    *context,
                                    const char  *variable_name,
			            int64_t      value);
void dsk_print_set_uint64          (DskPrint    *context,
                                    const char  *variable_name,
			            uint64_t     value);
void dsk_print_set_template_string (DskPrint    *context,
                                    const char  *variable_name,
			            const char  *template_string);
void dsk_print_set_quoted_string   (DskPrint    *context,
                                    const char  *variable_name,
			            const char  *template_string);

/* Making a context of variables that can be popped in one quick go */
void dsk_print_push (DskPrint *context);
void dsk_print_pop  (DskPrint *context);

void dsk_print                     (DskPrint    *context,
                                    const char  *template_string);

