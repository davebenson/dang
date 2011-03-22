#include "../dsk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static dsk_boolean cmdline_verbose = DSK_FALSE;

#define assert_or_error(call)                                      \
{if (call) {}                                                      \
  else { dsk_die ("running %s failed (%s:%u): %s",                 \
                  #call, __FILE__, __LINE__,                       \
                  error ? error->message : "no error message"); }}

typedef struct _KeyValue KeyValue;
struct _KeyValue
{
  char *key, *value;
};
static char *
generate_random_string (unsigned  min,
                        unsigned  range)
{
  unsigned len = min + rand () % range;
  char *rv = dsk_malloc (len + 1);
  unsigned i;
  for (i = 0; i < len; i++)
    rv[i] = "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789" [rand () % 62];
  rv[i] = 0;
  return rv;
}

static void
generate_random_key_value (KeyValue *kv)
{
  kv->key = generate_random_string (3, 10);
  kv->value = generate_random_string (3, 10);
}

static int compare_key_value (const void *a, const void *b)
{
  const char *ka = ((KeyValue*)a)->key;
  const char *kb = ((KeyValue*)b)->key;
  return strcmp (ka, kb);
}

static void destruct_key_value (KeyValue *kv)
{
  dsk_free (kv->key);
  dsk_free (kv->value);
}

/* values will be sorted upon return */
static void
generate_random_key_values (unsigned n,
                            KeyValue *kv)
{
  unsigned i;
  for (i = 0; i < n; i++)
    generate_random_key_value (kv + i);

redo_sort:
  qsort (kv, n, sizeof (KeyValue), compare_key_value);
  for (i = 0; i + 1 < n; i++)
    if (strcmp (kv[i].key, kv[i+1].key) == 0)
      break;
  if (i + 1 < n)
    {
      unsigned o;
      o = i;
      i++;
      while (i < n)
        {
          if (strcmp (kv[o].key, kv[i].key) != 0)
            kv[++o] = kv[i];
          i++;
        }
      i = o + 1;
      for (i = o + 1; i < n; i++)
        {
          destruct_key_value (kv + i);
          generate_random_key_value (kv + i);
        }
      goto redo_sort;
    }
}

typedef enum
{
  RAND_TEST_ORDERING_SORTED,
  RAND_TEST_ORDERING_RANDOM,
  RAND_TEST_ORDERING_REVERSED
} RandTestOrdering;

static unsigned *
generate_ordering (unsigned rand_test_size,
                   RandTestOrdering ordering)
{
  unsigned *rv;
  unsigned i;
  switch (ordering)
    {
    case RAND_TEST_ORDERING_SORTED:
      rv = dsk_malloc (sizeof (unsigned) * rand_test_size);
      for (i = 0; i < rand_test_size; i++)
        rv[i] = i;
      return rv;
    case RAND_TEST_ORDERING_REVERSED:
      rv = dsk_malloc (sizeof (unsigned) * rand_test_size);
      for (i = 0; i < rand_test_size; i++)
        rv[i] = rand_test_size - 1 - i;
      return rv;
    case RAND_TEST_ORDERING_RANDOM:
      rv = generate_ordering (rand_test_size, RAND_TEST_ORDERING_SORTED);

      /* not a perfect scramble, but good enough for this test code i hope */
      for (i = 0; i < rand_test_size; i++)
        {
          unsigned sw_a = rand () % rand_test_size;
          unsigned sw = rv[i];
          rv[i] = rv[sw_a];
          rv[sw_a] = sw;
        }
      return rv;
    }
  dsk_assert_not_reached ();
  return NULL;
}

static void
test_table_simple (unsigned rand_test_size,
                     RandTestOrdering ordering)
{
  DskTable *table;
  DskTableConfig config = DSK_TABLE_CONFIG_DEFAULT;
  unsigned value_len;
  const uint8_t *value_data;
  DskError *error = NULL;
  KeyValue *kvs;
  unsigned *order;
  unsigned i;
  table = dsk_table_new (&config, &error);
  if (table == NULL)
    dsk_die ("error creating default table: %s", error->message);
  dsk_assert (!dsk_table_lookup (table, 1, (uint8_t*) "a",
                                 &value_len, &value_data, &error));
  dsk_assert (error == NULL);
  assert_or_error (dsk_table_insert (table,  1, (uint8_t*) "a", 1, (uint8_t*) "z", &error));
  dsk_assert (!dsk_table_lookup (table, 1, (uint8_t*) "b",
                                 &value_len, &value_data, &error));
  dsk_assert (error == NULL);
  assert_or_error (dsk_table_lookup (table,  1, (uint8_t*) "a",
                                     &value_len, &value_data, &error));
  dsk_assert (value_len == 1);
  dsk_assert (value_data[0] == 'z');

  kvs = dsk_malloc (sizeof (KeyValue) * rand_test_size);
  generate_random_key_values (rand_test_size, kvs);
  order = generate_ordering (rand_test_size, ordering);

  for (i = 0; i < rand_test_size; i++)
    {
      KeyValue kv = kvs[order[i]];
      unsigned kl = strlen (kv.key);
      unsigned vl = strlen (kv.value);
      assert_or_error (dsk_table_insert (table, 
                                         kl, (uint8_t*) (kv.key),
                                         vl, (uint8_t*) (kv.value),
                                         &error));
    }
  for (i = 0; i < rand_test_size; i++)
    {
      KeyValue kv = kvs[i];
      unsigned kl = strlen (kv.key);
      unsigned vl;
      const uint8_t *value;
      assert_or_error (dsk_table_lookup (table, 
                                         kl, (uint8_t*) (kv.key),
                                         &vl, &value,
                                         &error));
      dsk_assert (vl == strlen (kv.value));
      dsk_assert (memcmp (kv.value, value, vl) == 0);
    }
  for (i = 0; i < rand_test_size; i++)
    destruct_key_value (kvs + i);
  dsk_free (kvs);
  dsk_free (order);

  dsk_table_destroy_erase (table);
}
static void
test_table_simple_small_sorted (void)
{
  test_table_simple (200, RAND_TEST_ORDERING_SORTED);
}
static void
test_table_simple_small_reversed (void)
{
  test_table_simple (200, RAND_TEST_ORDERING_REVERSED);
}
static void
test_table_simple_small_random (void)
{
  test_table_simple (200, RAND_TEST_ORDERING_RANDOM);
}
static void
test_table_simple_medium_sorted (void)
{
  test_table_simple (20000, RAND_TEST_ORDERING_SORTED);
}
static void
test_table_simple_medium_reversed (void)
{
  test_table_simple (20000, RAND_TEST_ORDERING_REVERSED);
}
static void
test_table_simple_medium_random (void)
{
  test_table_simple (20000, RAND_TEST_ORDERING_RANDOM);
}


static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple small, sorted-input database test", test_table_simple_small_sorted },
  { "simple small, reversed-input database test", test_table_simple_small_reversed },
  { "simple small, random-input database test", test_table_simple_small_random },
  { "simple medium, sorted-input database test", test_table_simple_medium_sorted },
  { "simple medium, reversed-input database test", test_table_simple_medium_reversed },
  { "simple medium, random-input database test", test_table_simple_medium_random },
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
