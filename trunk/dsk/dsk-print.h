// EXPERIMENT: vararg-free printing

/* DskPrint *codegen = dsk_print_new_fp (stdout, NULL);
   dsk_print_set_string (codegen, "key", "value");
   dsk_print (codegen, "this is $key!\n");
   dsk_print_context_free (codegen);
 */

typedef dsk_boolean (*DskPrintAppendFunc) (unsigned   length,
                                           const uint8_t *data,
                                           void      *append_data,
					   DskError **error);

typedef struct _DskPrint DskPrint;

DskPrint *dsk_print_new    (DskPrintAppendFunc append,
                            void              *append_data,
			    DskDestroyNotify   append_data_destroy);
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
void dsk_print_push (DskPrint *context);
void dsk_print_pop  (DskPrint *context);

void dsk_print                     (DskPrint    *context,
                                    const char  *template_string);

typedef enum
{
  /* This is quoted in the same manner that strings are in C.
     we quote the following:   \a \b \f \n \r \t \v
     all other nonprintables and non-ansi (ie above 127)
     are escaped as octal.  If the octal escaping is followed by a digit,
     use three octal numbers; if followed by a non-digit, use as
     few charcters as the character core permits. */
  DSK_PRINT_STRING_C_QUOTED,

  /* Use hex characters to encode the data */
  DSK_PRINT_STRING_HEX,

  /* Use hex characters, each byte boundary separated by spaces,
     to encode the data */
  DSK_PRINT_STRING_HEX_PAIRS,

  /* Dump the binary with non-printables transformed to question-marks */
  DSK_PRINT_STRING_MYSTERIOUSLY,

  /* Just dump the binary - may cause UTF-8 problems. */
  DSK_PRINT_STRING_RAW

} DskPrintStringQuoting;

/* Making a context of variables that can be popped in one quick go */
void dsk_print_set_quoted_string   (DskPrint    *context,
                                    const char  *variable_name,
			            const char  *raw_string,
                                    DskPrintStringQuoting quoting_method);
void dsk_print_set_quoted_binary   (DskPrint    *context,
                                    const char  *variable_name,
                                    size_t       raw_string_length,
			            const char  *raw_string,
                                    DskPrintStringQuoting quoting_method);
void dsk_print_set_quoted_buffer   (DskPrint    *context,
                                    const char  *variable_name,
			            const DskBuffer *buffer,
                                    DskPrintStringQuoting quoting_method);

