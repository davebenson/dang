
typedef dsk_boolean (*DskHookFunc)    (void       *object,
                                       void       *callback_data);
typedef void        (*DskHookDestroy) (void       *callback_data);
typedef void        (*DskHookSetPoll) (void       *object,
                                       dsk_boolean is_trapped);
typedef void        (*DskHookObjectFunc) (void *object);


typedef struct _DskHookTrap DskHookTrap;
typedef struct _DskHook DskHook;

struct _DskHookFuncs
{
  DskHookObjectFunc ref;
  DskHookObjectFunc unref;
  DskHookSetPoll set_poll;
};

struct _DskHookTrap
{
  DskHookFunc callback;		/* != NULL */
  void *callback_data;
  DskHookDestroy callback_data_destroy;
  DskHook *owner;
  DskHookTrap *next;
  unsigned is_notifying : 1;
  unsigned destroy_in_notify : 1;
  unsigned short block_count;
};
  
struct _DskHook
{
  void *object;
  unsigned char is_idle_notify : 1;
  unsigned char is_destroyed : 1;
  unsigned char is_notifying : 1;
  unsigned char magic;
  unsigned short trap_count;
  DskHookTrap trap;
  DskHook *idle_prev, *idle_next;
  DskHookFuncs *funcs;
};

#define DSK_HOOK_MAGIC          0x81

DSK_INLINE_FUNC dsk_boolean  dsk_hook_is_trapped   (DskHook       *hook);
DSK_INLINE_FUNC DskHookTrap *dsk_hook_trap         (DskHook       *hook,
                                                    DskHookFunc    func,
						    void          *hook_data,
						    DskHookDestroy destroy);
                void         dsk_hook_trap_destroy (DskHookTrap   *trap);
DSK_INLINE_FUNC void         dsk_hook_trap_block   (DskHookTrap   *trap);
DSK_INLINE_FUNC void         dsk_hook_trap_unblock (DskHookTrap   *trap);

/* for use by the underlying polling mechanism
 * (for hooks not using idle-notify)
 */
DSK_INLINE_FUNC DskHook     *dsk_hook_new          (void          *object);
DSK_INLINE_FUNC void         dsk_hook_set_poll_func(DskHook       *hook,
                                                    DskHookSetPoll set_poll);
DSK_INLINE_FUNC void         dsk_hook_set_idle_notify(DskHook       *hook,
                                                      dsk_boolean    idle_notify);
                void         dsk_hook_notify       (DskHook       *hook);
                void         dsk_hook_destroy      (DskHook       *hook);

#if DSK_CAN_INLINE || DSK_IMPLEMENT_INLINES
DSK_INLINE_FUNC dsk_boolean dsk_hook_is_trapped (DskHook *hook)
{
  return hook->trap_count > 0;
} 
DSK_INLINE_FUNC void _dsk_hook_incr_trap_count         (DskHook       *hook)
{
  if (++(hook->trap_count) == 1)
    _dsk_hook_trap_count_nonzero (hook);
} 
DSK_INLINE_FUNC void _dsk_hook_decr_trap_count         (DskHook       *hook)
{
  if (--(hook->trap_count) == 0)
    _dsk_hook_trap_count_zero (hook);
} 

DSK_INLINE_FUNC DskHook     *dsk_hook_new          (void          *object)
{
  DskHook *rv = dsk_mem_pool_fixed_alloc0 (&dsk_hook_pool);
  rv->object = object;
  rv->magic = DSK_HOOK_MAGIC;
  return rv;
}

DSK_INLINE_FUNC DskHookTrap *dsk_hook_trap         (DskHook       *hook,
                                                    DskHookFunc    func,
						    void          *hook_data,
						    DskHookDestroy destroy)
{
  dsk_assert (func != NULL);
  if (hook->trap.func == NULL)
    {
      trap = &hook->trap;
    }
  else
    {
      DskHookTrap *last = &hook->trap;
      trap = dsk_mem_pool_fixed_alloc (&dsk_hook_trap_pool);
      while (last->next != NULL)
        last = last->next;
      last->next = trap;
      trap->next = NULL;
    }
  
  trap->callback = callback;
  trap->callback_data = callback_data;
  trap->callback_data_destroy = callback_data_destroy;
  trap->owner = hook;
  trap->is_notifying = 0;
  trap->block_count = 0;
  _dsk_hook_incr_trap_count (hook);

  return trap;
}
DSK_INLINE_FUNC void         dsk_hook_trap_block   (DskHookTrap   *trap)
{
  dsk_assert (trap->callback != NULL);
  dsk_assert (trap->block_count != 0xffff);
  if (++(trap->block_count) == 1)
    _dsk_hook_decr_trap_count (trap->owner);
}

DSK_INLINE_FUNC void         dsk_hook_trap_unblock (DskHookTrap   *trap)
{
  dsk_assert (trap->callback != NULL);
  dsk_assert (trap->block_count != 0);
  if (--(trap->block_count) == 0)
    _dsk_hook_incr_trap_count (trap->owner);
}

#endif


