
void _dsk_hook_trap_count_nonzero (DskHook *hook)
{
  if (hook->is_idle_notify)
    {
      /* put into idle-notify list */
      hook->idle_prev = dsk_hook_idle_last;
      hook->idle_next = NULL;
      dsk_hook_idle_last = hook;
      if (dsk_hook_idle_first == NULL)
        dsk_hook_idle_first = hook;
    }
  if (hook->set_poll_func != NULL)
    hook->set_poll_func (hook->object, DSK_TRUE);
}
void _dsk_hook_trap_count_zero (DskHook *hook)
{
  if (hook->is_idle_notify)
    {
      /* remove from idle-notify list */
      if (hook->idle_prev == NULL)
        dsk_hook_idle_first = hook->idle_next;
      else
        hook->idle_prev->idle_next = hook->idle_next;
      if (hook->idle_next == NULL)
        dsk_hook_idle_last = hook->idle_prev;
      else
        hook->idle_next->idle_prev = hook->idle_prev;
    }
  if (hook->set_poll_func != NULL)
    hook->set_poll_func (hook->object, DSK_FALSE);
}
