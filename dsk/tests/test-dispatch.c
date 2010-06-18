#include <stdio.h>
#include <signal.h>
#include "../dsk.h"

static dsk_boolean cmdline_verbose = DSK_FALSE;

static void set_boolean_true (void *data) { * (dsk_boolean *) data = DSK_TRUE; }

static void
test_signal_handling (void)
{
  DskDispatchSignal *sig;
  dsk_boolean sig_triggered = DSK_FALSE;
  dsk_boolean timer_triggered = DSK_FALSE;
  sig = dsk_main_add_signal (SIGUSR1, set_boolean_true, &sig_triggered);
  dsk_main_add_timer_millis (50, set_boolean_true, &timer_triggered);
  while (!timer_triggered)
    dsk_main_run_once ();
  dsk_assert (!sig_triggered);
  raise (SIGUSR1);
  while (!sig_triggered)
    dsk_main_run_once ();
  sig_triggered = DSK_FALSE;
  raise (SIGUSR1);
  while (!sig_triggered)
    dsk_main_run_once ();
  dsk_main_remove_signal (sig);
}

static struct 
{
  const char *name;
  void (*test)(void);
} tests[] =
{
  { "signal handling", test_signal_handling },
};
int main(int argc, char **argv)
{
  unsigned i;

  dsk_cmdline_init ("test dispatch code",
                    "Test the dispatch code",
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
