#include "dsk.h"

void              dsk_main_watch_fd        (DskFileDescriptor   fd,
                                            unsigned            events,
                                            DskFDFunc           callback,
                                            void               *callback_data)
{
  dsk_assert (fd >= 0);
  dsk_dispatch_watch_fd (dsk_dispatch_default (),
                         fd, events, callback, callback_data);
}

void              dsk_main_close_fd        (DskFileDescriptor   fd)
{
  dsk_dispatch_close_fd (dsk_dispatch_default (), fd);
}
void              dsk_main_fd_closed       (DskFileDescriptor   fd)
{
  dsk_dispatch_fd_closed (dsk_dispatch_default (), fd);
}

DskDispatchTimer *dsk_main_add_timer       (unsigned            timeout_secs,
                                            unsigned            timeout_usecs,
                                            DskTimerFunc        func,
                                            void               *func_data)
{
  return dsk_dispatch_add_timer (dsk_dispatch_default (),
                                 timeout_secs, timeout_usecs, func, func_data);
}

DskDispatchTimer *dsk_main_add_timer_millis(unsigned            milliseconds,
                                            DskTimerFunc        func,
                                            void               *func_data)
{
  return dsk_dispatch_add_timer_millis (dsk_dispatch_default (),
                                        milliseconds, func, func_data);
}

void
dsk_main_adjust_timer    (DskDispatchTimer   *timer,
                          unsigned            timeout_secs,
                          unsigned            timeout_usecs)
{
  dsk_dispatch_adjust_timer (timer, timeout_secs, timeout_usecs);
}

void              dsk_main_adjust_timer_millis (DskDispatchTimer *timer,
                                            unsigned            milliseconds)
{
  dsk_dispatch_adjust_timer_millis (timer, milliseconds);
}

void              dsk_main_remove_timer    (DskDispatchTimer   *timer)
{
  dsk_dispatch_remove_timer (timer);
}

DskDispatchIdle  *dsk_main_add_idle        (DskIdleFunc         func,
                                            void               *func_data)
{
  return dsk_dispatch_add_idle (dsk_dispatch_default (), func, func_data);
}
void              dsk_main_remove_idle     (DskDispatchIdle    *idle)
{
  dsk_dispatch_remove_idle (idle);
}


/* program termination (terminate when ref-count gets to 0);
 * many programs leave 0 refs the whole time.
 */
static unsigned main_n_refs = 0;
static int main_exit_status = -1;
void              dsk_main_add_object      (void               *object)
{
  dsk_main_add_ref ();
  dsk_object_trap_finalize (object, (DskDestroyNotify) dsk_main_remove_ref, NULL);
}

void              dsk_main_add_ref         (void)
{
  ++main_n_refs;
}

void              dsk_main_remove_ref      (void)
{
  dsk_assert (main_n_refs > 0);
  if (--main_n_refs == 0)
    dsk_main_quit ();
}

int
dsk_main_run             (void)
{
  while (main_exit_status < 0)
    dsk_main_run_once ();
  return main_exit_status;
}

void              dsk_main_exit            (int                exit_status)
{
  main_exit_status = exit_status;
}

void              dsk_main_quit            (void)
{
  main_exit_status = 0;
}
