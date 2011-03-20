

/* --- utf-8 string handling --- */
void dsk_utf8_skip_whitespace (const char **p_str);

unsigned dsk_utf8_encode_unichar (char *buf_out,
                                  uint32_t unicode_value);
