
extern unsigned char dsk_ascii_chartable[256];

/* NOTE: NUL is NOT a space */
#define dsk_ascii_isspace(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 1)
#define dsk_ascii_isupper(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 2)
#define dsk_ascii_islower(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 4)
#define dsk_ascii_isdigit(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 8)
#define dsk_ascii_isxdigit(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 16)

/* isalpha = isupper || is_lower */
#define dsk_ascii_isalpha(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 6)
/* isalpha = isupper || is_lower || is_digit */
#define dsk_ascii_isalnum(c)  (dsk_ascii_chartable[(unsigned char)(c)] & 14)

int dsk_ascii_xdigit_value (int c);
int dsk_ascii_digit_value (int c);

#define DSK_ASCII_SKIP_SPACE(ptr) \
      do { while (dsk_ascii_isspace (*(ptr))) ptr++; } while(0)
#define DSK_ASCII_SKIP_NONSPACE(ptr) \
      do { while (*(ptr) && !dsk_ascii_isspace (*(ptr))) (ptr)++; } while(0)
