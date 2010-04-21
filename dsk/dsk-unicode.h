
extern unsigned char dsk_ascii_chartable[256];

/* NOTE: NUL is NOT a space */
#define dsk_ascii_isspace(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 1)
#define dsk_ascii_isalpha(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 2)
#define dsk_ascii_isdigit(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 4)
#define dsk_ascii_isxdigit(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 8)



#define DSK_ASCII_SKIP_SPACE(ptr) \
      do { while (dsk_ascii_isspace (*(ptr))) ptr++; } while(0)
#define DSK_ASCII_SKIP_NONSPACE(ptr) \
      do { while (*(ptr) && !dsk_ascii_isspace (*(ptr))) (ptr)++; } while(0)
