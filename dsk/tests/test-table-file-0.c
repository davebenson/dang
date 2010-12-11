#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "../dsk.h"

static char *test_dir = NULL;
static int   test_dir_fd = -1;
static dsk_boolean cmdline_verbose = DSK_FALSE;

static void
test_simple_write_read (void)
{
  DskTableFileOptions opts = DSK_TABLE_FILE_OPTIONS_DEFAULT;
  DskError *error = NULL;
  opts.openat_fd = test_dir_fd;
  opts.base_filename = "base";

  DskTableFileWriter *writer = dsk_table_file_writer_new (&opts, &error);
  if (writer == NULL)
    dsk_die ("%s", error->message);
  if (!dsk_table_file_write (writer, 1, (uint8_t*) "a", 1, (uint8_t*) "A", &error)
   || !dsk_table_file_write (writer, 1, (uint8_t*) "b", 1, (uint8_t*) "B", &error)
   || !dsk_table_file_write (writer, 1, (uint8_t*) "c", 1, (uint8_t*) "C", &error))
    dsk_die ("error writing: %s", error->message);
  if (!dsk_table_file_writer_close (writer, &error))
    dsk_die ("error closing writer: %s", error->message);

  DskTableFileReader *reader = dsk_table_file_reader_new (&opts, &error);
  if (reader == NULL)
    dsk_die ("error creating reader: %s", error->message);
  dsk_assert (!reader->at_eof);
  dsk_assert (reader->key_length == 1);
  dsk_assert (reader->key_data[0] == 'a');
  dsk_assert (reader->value_length == 1);
  dsk_assert (reader->value_data[0] == 'A');
  if (!dsk_table_file_reader_advance (reader, &error))
    dsk_die ("error advancing reader: %s", error->message);
  dsk_assert (!reader->at_eof);
  dsk_assert (reader->key_length == 1);
  dsk_assert (reader->key_data[0] == 'b');
  dsk_assert (reader->value_length == 1);
  dsk_assert (reader->value_data[0] == 'B');
  if (!dsk_table_file_reader_advance (reader, &error))
    dsk_die ("error advancing reader: %s", error->message);
  dsk_assert (!reader->at_eof);
  dsk_assert (reader->key_length == 1);
  dsk_assert (reader->key_data[0] == 'c');
  dsk_assert (reader->value_length == 1);
  dsk_assert (reader->value_data[0] == 'C');
  if (dsk_table_file_reader_advance (reader, &error))
    dsk_die ("expected EOF");
  if (error)
    dsk_die ("expected EOF, got error: %s", error->message);
  dsk_table_file_reader_destroy (reader);
}

static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "simple writing/reading", test_simple_write_read },
};

int main(int argc, char **argv)
{
  unsigned i;

  dsk_cmdline_init ("test CGI parsing",
                    "Test the CGI Parsing Code",
                    NULL, 0);
  dsk_cmdline_add_boolean ("verbose", "extra logging", NULL, 0,
                           &cmdline_verbose);
  dsk_cmdline_process_args (&argc, &argv);

  char test_dir_buf[256];
  snprintf (test_dir_buf, sizeof (test_dir_buf),
            "test-table-file-%u-%u", (unsigned)time(NULL), (unsigned)getpid());
  test_dir = test_dir_buf;
  if (mkdir (test_dir, 0755) < 0)
    dsk_die ("error making test directory (%s): %s",
             test_dir, strerror (errno));
  test_dir_fd = open (test_dir, O_RDONLY);
  if (test_dir_fd < 0)
    dsk_die ("error opening dir %s: %s", test_dir, strerror (errno));

  for (i = 0; i < DSK_N_ELEMENTS (tests); i++)
    {
      fprintf (stderr, "Test: %s... ", tests[i].name);
      tests[i].test ();
      fprintf (stderr, " done.\n");
    }
  dsk_cleanup ();
  return 0;
}
