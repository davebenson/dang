#include "dsk-hook.h"

static DskDispatchIdle *idle_handler = NULL;

DskHookFuncs dsk_hook_funcs_default =
{
  (DskHookObjectFunc) dsk_object_ref_f,
  (DskHookObjectFunc) dsk_object_unref_f,
  NULL
};

void
dsk_hook_notify (DskHook *hook)
{
  void *object = hook->object;
  DskHookFuncs *funcs = hook->funcs;
  dsk_assert (hook->magic == DSK_HOOK_MAGIC);
  if (hook->is_notifying || hook->is_cleared)
    return;
  hook->is_notifying = 1;
  if (funcs->ref != NULL)
    funcs->ref (object);
  if (hook->trap.callback != NULL && hook->trap.block_count == 0)
    {
      void *data = hook->trap.callback_data;
      hook->trap.is_notifying = 1;
      if (!hook->trap.callback (hook->object, data) || hook->trap.destroy_in_notify)
        {
          DskHookDestroy destroy = hook->trap.callback_data_destroy;
          hook->trap.callback = NULL;
          hook->trap.callback_data = NULL;
          hook->trap.callback_data_destroy = NULL;
          if (destroy)
            destroy (data);
          hook->trap.destroy_in_notify = 0;
        }
      hook->trap.is_notifying = 0;
    }
  if (DSK_UNLIKELY (hook->trap.next != NULL))
    {
      DskHookTrap *trap;
      dsk_boolean must_prune = DSK_FALSE;
      for (trap = hook->trap.next;
           trap != NULL && !hook->destroy_in_notify;
           trap = trap->next)
        {
          trap->is_notifying = 1;
          if (!trap->callback (hook->object, data) || trap->destroy_in_notify)
            {
              DskHookDestroy destroy = trap->callback_data_destroy;
              if (trap->block_count == 0 && trap->callback != NULL)
                {
                  if (--(hook->trap_count) == 0)
                    _dsk_hook_trap_count_zero (trap->owner);
                }
              trap->callback = NULL;
              trap->callback_data = NULL;
              trap->callback_data_destroy = NULL;
              if (destroy)
                destroy (data);
              must_prune = TRUE;
            }
          trap->is_notifying = 0;
        }
      if (must_prune)
        {
          DskHookTrap **ptrap = &hook->trap.next;
          while (*ptrap != NULL)
            {
              if ((*ptrap)->callback == NULL)
                {
                  DskHookTrap *to_free = *ptrap;
                  *ptrap = to_free->next;
                  dsk_mem_pool_fixed_free (&dsk_hook_trap_pool, to_free);
                }
              else
                ptrap = &((*ptrap)->next);
            }
        }
    }
  hook->is_notifying = 0;
  if (hook->clear_in_notify)
    dsk_hook_clear (hook);
  if (funcs->unref != NULL)
    funcs->unref (object);
}

void
dsk_hook_trap_destroy (DskHookTrap   *trap)
{
  /* If the trap itself is notifying, we handle it in dsk_hook_notify() */
  if (trap->is_notifying)
    {
      trap->destroy_in_notify = 1;
      return;
    }

  /* If the trap is active, we should decrement the trap count,
   * possibly disabling notification. */
  if (trap->block_count == 0 && trap->callback)
    {
      if (--(trap->owner->trap_count) == 0)
        _dsk_hook_trap_count_zero (trap->owner);
    }
  if (trap->callback_data_destroy)
    trap->callback_data_destroy (trap->callback_data);
  trap->callback = NULL;
  trap->callback_data = NULL;
  trap->callback_data_destroy = NULL;
  if (&(trap->owner->trap) != trap)
    {
      /* remove from list and free */
      DskHookTrap **pt;
      for (pt = &trap->owner->trap.next; *pt != trap; pt = &((*pt)->next))
        dsk_assert (*pt != NULL);
      *pt = trap->next;
      dsk_mem_pool_fixed_free (&dsk_hook_trap_pool, trap);
    }


static dsk_boolean
run_idle_notifications (void *data)
{
  DskHook idle_notify_guard;
  DSK_UNUSED (data);
  if (dsk_hook_idle_first == NULL)
    {
      idle_handler = NULL;
      return FALSE;
    }
  idle_notify_guard.idle_prev = dsk_hook_idle_last;
  idle_notify_guard.idle_next = NULL;
  dsk_hook_idle_last->idle_next = &idle_notify_guard;
  dsk_hook_idle_last = &idle_notify_guard;

  while (dsk_hook_idle_first != &idle_notify_guard)
    {
      /* move idle handler to end of list */
      DskHook *at = dsk_hook_idle_first;
      dsk_hook_idle_first = dsk_hook_idle_first->idle_next;
      dsk_hook_idle_first->idle_prev = NULL;
      dsk_hook_idle_last->idle_next = at;
      at->idle_prev = dsk_hook_idle_last;
      at->idle_next = NULL;
      dsk_hook_idle_last = at;

      /* invoke it */
      dsk_hook_notify (at);
    }
  if (dsk_hook_idle_first->idle_next == NULL)
    {
      dsk_hook_idle_first = dsk_hook_idle_last = NULL;
    }
  else
    {
      dsk_hook_idle_first->idle_next->idle_prev = NULL;
      dsk_hook_idle_first = dsk_hook_idle_first->idle_next;
    }
}
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
      if (idle_handler == NULL)
        idle_handler = dsk_dispatch_add_idle (dsk_dispatch_default (),
                                              run_idle_notifications,
                                              NULL);
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

void
dsk_hook_clear      (DskHook       *hook)
{
  DskHookTrap *trap;
  dsk_assert (!hook->is_cleared);
  dsk_assert (hook->magic == DSK_HOOK_MAGIC);
  if (hook->is_notifying)
    {
      hook->clear_in_notify = 1;
      return;
    }
  hook->is_cleared = 1;
  hook->magic = 1;
  if (hook->trap.destroy)
    hook->trap.destroy (hook->trap.data);
  trap = hook->trap.next
  while (trap != NULL)
    {
      DskHookTrap *next = trap->next;
      if (trap->destroy != NULL)
        trap->destroy (trap->data);
      dsk_mem_pool_fixed_free (&dsk_hook_trap_pool, trap);
      trap = next;
    }
}