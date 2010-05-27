#include "dsk-common.h"
#include "dsk-unicode.h"

unsigned char dsk_ascii_chartable[256] = 
{
#include "dsk-ascii-chartable.inc"
};

/* xdigit_values and digit_values are machine-generated */
#include "dsk-digit-chartables.inc"

int dsk_ascii_xdigit_value (int c)
{
  return xdigit_values[(uint8_t)c];
}

int dsk_ascii_digit_value (int c)
{
  return digit_values[(uint8_t)c];
}
#include "dsk-byte-name-table.inc"
const char *dsk_ascii_byte_name(unsigned char byte)
{
  return byte_name_str + (byte_name_offsets[byte]);
}
