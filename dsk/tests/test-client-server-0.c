#include "../dsk.h"

#define DEFAULT_UNIX_PORT   "test-client-server-0.socket"

/* --- implement a simple echo server --- */
...


static void
create_server (void)
{
...
}
static void
kill_server (void)
{
...
}
/* --- test utilities --- */
static DskClientStreamSink *client_sink;
static DskClientStreamSource *client_source;
static DskClientStream *client_stream;

static void
create_client (void)
{
...
}
static void
kill_client (void)
{
...
}
static void
test_client_defunct (void)
{
...
}

int main(int argc, char **argv)
{
  /* command-line options */
  ...

  /* create client */
  create_client ();
  test_client_defunct ();

  /* create server */
  create_server ();
  test_echo_server_up ();
  test_echo_server_up ();
  test_echo_server_up ();

  /* destroy server */
  kill_server ();
  test_client_defunct ();

  /* re-create server */
  create_server ();
  pause_a_moment ();
  kill_server ();
  test_client_defunct ();

  kill_client ();
  return 0;
}
