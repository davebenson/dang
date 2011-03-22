#include "../dsk.h"
#include <stdio.h>

static dsk_boolean cmdline_verbose = DSK_FALSE;

#define assert_or_error(call)                                      \
{if (call) {}                                                      \
  else { dsk_die ("running %s failed (%s:%u): %s",                 \
                  #call, __FILE__, __LINE__,                       \
                  error ? error->message : "no error message"); }}

static void
test_table_simple_0 (void)
{
  DskTable *table;
  DskTableConfig config = DSK_TABLE_CONFIG_DEFAULT;
  unsigned value_len;
  const uint8_t *value_data;
  DskError *error = NULL;
  table = dsk_table_new (&config, &error);
  if (table == NULL)
    dsk_die ("error creating default table: %s", error->message);
  dsk_assert (!dsk_table_lookup (table, 1, (uint8_t*) "a",
                                 &value_len, &value_data, &error));
  dsk_assert (error == NULL);
  assert_or_error (dsk_table_insert (table,  1, (uint8_t*) "a", 1, (uint8_t*) "z", &error));
  dsk_assert (!dsk_table_lookup (table, 1, (uint8_t*) "a",
                                 &value_len, &value_data, &error));
  assert_or_error (dsk_table_lookup (table,  1, (uint8_t*) "a",
                                     &value_len, &value_data, &error));
  dsk_assert (value_len == 1);
  dsk_assert (value_data[0] == 'z');
  dsk_table_destroy_erase (table);
}


static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple small database test", test_table_simple_0 },
};

int main(int argc, char **argv)
{
  unsigned i;

  dsk_cmdline_init ("test DskTable",
                    "Test DskTable, our small persistent key-value table",
                    NULL, 0);
  dsk_cmdline_add_boolean ("verbose", "extra logging", NULL, 0,
                           &cmdline_verbose);
  dsk_cmdline_process_args (&argc, &argv);

  for (i = 0; i < DSK_N_ELEMENTS (tests); i++)
    {
      fprintf (stderr, "Test: %s... ", tests[i].name);
      tests[i].test ();
      fprintf (stderr, " done.\n");
    }
  dsk_cleanup ();
  return 0;
}
