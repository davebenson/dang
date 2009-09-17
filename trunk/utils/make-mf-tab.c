/* Take a sorted list of metafunctions on stdin,
 * output code that can be included in dang-metafunction
 * to do an optimized mf lookup. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

int main()
{
  char **mf = NULL;
  unsigned n_mf = 0;
  char buf[1024];
  unsigned i;
  char c;

  while (fgets (buf, sizeof (buf), stdin) != NULL)
    {
      char *nl;
      nl = strchr (buf, '\n');
      assert (nl);
      *nl = 0;
      if (n_mf == 0)
        mf = malloc (sizeof (char*));
      else
        mf = realloc (mf, sizeof (char*) * (n_mf + 1));
      assert (mf != NULL);
      mf[n_mf] = strdup (buf);
      assert (mf[n_mf] != NULL);
      n_mf++;
    }
  for (i = 0; i < n_mf; i++)
    printf ("extern DangMetafunction _dang_metafunction__%s;\n", mf[i]);
  printf ("\nstatic DangMetafunction *mf_table[%u] = {\n", n_mf);
  for (i = 0; i < n_mf; i++)
    printf ("  &_dang_metafunction__%s,\n", mf[i]);
  printf ("};\n\n");

  printf ("static MFInitialCharInfo mf_initial_char_info[26] = {\n");
  i = 0;
  for (c = 'a'; c <= 'z'; c++)
    {
      unsigned start_i = i;
      while (i < n_mf && mf[i][0] == c)
        i++;
      printf ("  { %u, %u },\n", start_i, i - start_i);
    }
  printf ("};\n");
  return 0;
}
