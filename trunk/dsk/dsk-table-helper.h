
int dsk_table_helper_openat (int openat_fd,
                             const char *base_filename,
                             const char *suffix,
                             unsigned    open_flags,
                             unsigned    open_mode,
                             DskError  **error);
int dsk_table_helper_pread  (int fd,
                             void *buf,
                             size_t len,
                             off_t offset);
