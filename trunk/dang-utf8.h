
typedef uint32_t dang_unichar;

/* At most 6 bytes will be written to 'buf'; no NUL.
   The number of bytes written is returned. */
unsigned dang_utf8_encode (dang_unichar unicode,
                           char    *buf);

dang_boolean dang_utf8_scan_char (char **at_inout,
                                  unsigned      max_len,
                                  dang_unichar *char_out,
                                  DangError   **error);

const char  *dang_utf8_next_char (const char *str);


dang_boolean dang_utf8_validate_str  (unsigned    len,
                                      const char *data,
                                      DangError **error);

unsigned dang_utf8_count_unichars        (unsigned            len,
                                          const char         *str);
void     dang_utf8_string_to_unichars    (unsigned            len,
                                          const char         *str,
                                          dang_unichar       *out);
unsigned dang_unichar_array_get_utf8_len (unsigned            n_chars,
                                          const dang_unichar *chars);

/* note: does NOT NUL-terminate */
void     dang_unichar_array_to_utf8      (unsigned            n_chars,
                                          const dang_unichar *chars,
                                          char               *utf8_out);
