#include "dsk.h"

#include "../gsklistmacros.h"
#include "debug.h"

static DskDispatchIdle *idle_handler = NULL;
static DskHook *dsk_hook_idle_first = NULL;
static DskHook *dsk_hook_idle_last = NULL;
#define GET_IDLE_HOOK_LIST()    DskHook *, dsk_hook_idle_first, dsk_hook_idle_last, idle_prev, idle_next

DskHookFuncs dsk_hook_funcs_default =
{
  (DskHookObjectFunc) dsk_object_ref_f,
  (DskHookObjectFunc) dsk_object_unref_f,
  NULL
};
DskMemPoolFixed dsk_hook_trap_pool = DSK_MEM_POOL_FIXED_STATIC_INIT (sizeof (DskHookTrap));

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
          void *destroy_data = hook->trap.callback_data;
          hook->trap.callback = NULL;
          hook->trap.callback_data = NULL;
          hook->trap.callback_data_destroy = NULL;
          if (destroy)
            destroy (destroy_data);
          hook->trap.destroy_in_notify = 0;
          if (--hook->trap_count == 0)
            _dsk_hook_trap_count_zero (hook);
        }
      hook->trap.is_notifying = 0;
    }
  if (DSK_UNLIKELY (hook->trap.next != NULL))
    {
      DskHookTrap *trap;
      dsk_boolean must_prune = DSK_FALSE;
      for (trap = hook->trap.next;
           trap != NULL;
           trap = trap->next)
        {
          trap->is_notifying = 1;
          if (!trap->callback (hook->object, trap->callback_data) || trap->destroy_in_notify)
            {
              DskHookDestroy destroy = trap->callback_data_destroy;
              void *destroy_data = hook->trap.callback_data;
              if (trap->block_count == 0 && trap->callback != NULL)
                {
                  if (--(hook->trap_count) == 0)
                    _dsk_hook_trap_count_zero (trap->owner);
                }
              trap->callback = NULL;
              trap->callback_data = NULL;
              trap->callback_data_destroy = NULL;
              if (destroy)
                destroy (destroy_data);
              must_prune = DSK_TRUE;
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
  if (dsk_debug_hooks)
    dsk_warning ("dsk_hook_trap_destroy: trap=%p, object=%p", trap, trap->owner->object);
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

  /* invoke destroy-notify */
  DskDestroyNotify destroy = trap->callback_data_destroy;
  void *data = trap->callback_data;

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

  if (destroy)
    destroy (data);
}


static void
run_idle_notifications (void *data)
{
  DskHook idle_notify_guard;
  DSK_UNUSED (data);
#if 0
  if (dsk_hook_idle_first == NULL)
    {
      if (idle_handler)
        {
          dsk_dispatch_remove_idle (idle_handler);
          idle_handler = NULL;
        }
      return;
    }
#endif
  idle_handler = NULL;
  idle_notify_guard.idle_prev = dsk_hook_idle_last;
  idle_notify_guard.idle_next = NULL;
  dsk_hook_idle_last->idle_next = &idle_notify_guard;
  dsk_hook_idle_last = &idle_notify_guard;

  while (dsk_hook_idle_first != &idle_notify_guard)
    {
      /* move idle handler to end of list */
      DskHook *at = dsk_hook_idle_first;
      GSK_LIST_REMOVE_FIRST (GET_IDLE_HOOK_LIST ());
      GSK_LIST_APPEND (GET_IDLE_HOOK_LIST (), at);

      /* invoke it */
      dsk_hook_notify (at);
    }
  GSK_LIST_REMOVE_FIRST (GET_IDLE_HOOK_LIST ());

  if (idle_handler == NULL && dsk_hook_idle_first != NULL)
    idle_handler = dsk_dispatch_add_idle (dsk_dispatch_default (),
                                          run_idle_notifications,
                                          NULL);
}
void _dsk_hook_trap_count_nonzero (DskHook *hook)
{
  if (dsk_debug_hooks)
    dsk_warning ("_dsk_hook_trap_count_nonzero: set_poll=%p",hook->funcs->set_poll);
  if (hook->is_idle_notify)
    {
      /* put into idle-notify list */
      GSK_LIST_APPEND (GET_IDLE_HOOK_LIST (), hook);
      if (idle_handler == NULL)
        idle_handler = dsk_dispatch_add_idle (dsk_dispatch_default (),
                                              run_idle_notifications,
                                              NULL);
    }
  if (hook->funcs->set_poll != NULL)
    hook->funcs->set_poll (hook->object, DSK_TRUE);
}
void _dsk_hook_trap_count_zero (DskHook *hook)
{
  if (hook->is_idle_notify)
    {
      /* remove from idle-notify list */
      GSK_LIST_REMOVE (GET_IDLE_HOOK_LIST (), hook);
    }
  if (hook->funcs->set_poll != NULL)
    hook->funcs->set_poll (hook->object, DSK_FALSE);
}

void _dsk_hook_add_to_idle_notify_list (DskHook *hook)
{
  if (idle_handler == NULL)
    idle_handler = dsk_dispatch_add_idle (dsk_dispatch_default (),
                                          run_idle_notifications,
                                          NULL);

  GSK_LIST_APPEND (GET_IDLE_HOOK_LIST (), hook);
}
void _dsk_hook_remove_from_idle_notify_list (DskHook *hook)
{
  GSK_LIST_REMOVE (GET_IDLE_HOOK_LIST (), hook);
}

void
dsk_hook_clear      (DskHook       *hook)
{
  DskHookTrap *trap;
  dsk_boolean was_trapped = hook->trap_count > 0;
  dsk_assert (!hook->is_cleared);
  dsk_assert (hook->magic == DSK_HOOK_MAGIC);
  if (hook->is_notifying)
    {
      hook->clear_in_notify = 1;
      return;
    }
  hook->is_cleared = 1;
  hook->magic = 1;
  if (hook->trap.callback_data_destroy)
    hook->trap.callback_data_destroy (hook->trap.callback_data);
  trap = hook->trap.next;
  while (trap != NULL)
    {
      DskHookTrap *next = trap->next;
      if (trap->callback_data_destroy != NULL)
        trap->callback_data_destroy (trap->callback_data);
      dsk_mem_pool_fixed_free (&dsk_hook_trap_pool, trap);
      trap = next;
    }
  hook->trap_count = 0;
  if (was_trapped)
    _dsk_hook_trap_count_zero (hook);
}