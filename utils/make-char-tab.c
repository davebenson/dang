#include <string.h>
#include <stdio.h>
int main(int argc, char **argv)
{
  unsigned i;
  unsigned char bytes[32];
  unsigned char *at = (unsigned char*)(argv[1]);
  memset (bytes, 0, 32);
  for (i = 0; at[i]; i++)
    bytes[at[i] / 8] |= (1 << (at[i] & 7));
  for (i = 0; i < 32; i++)
    printf ("0x%02x,%c", bytes[i], i % 8 == 7 ? '\n' : ' ');
  return 0;
}


