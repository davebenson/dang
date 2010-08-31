
char       *dsk_file_get_contents (const char *filename,
                                   size_t     *size_out,
			           DskError  **error);
dsk_boolean dsk_file_set_contents (const char *filename,
                                   size_t      size,
                                   uint8_t    *contents,
			           DskError  **error);

dsk_boolean dsk_file_test_exists  (const char *filename);
