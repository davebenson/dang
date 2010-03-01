#ifndef __DSK_DISPATCH_H_
#define __DSK_DISPATCH_H_

typedef struct _DskDispatch DskDispatch;
typedef struct _DskDispatchTimer DskDispatchTimer;
typedef struct _DskDispatchIdle DskDispatchIdle;

typedef enum
{
  DSK_EVENT_READABLE = (1<<0),
  DSK_EVENT_WRITABLE = (1<<1)
} Dsk_Events;

/* Create or destroy a Dispatch */
DskDispatch        *dsk_dispatch_new (void);
void                dsk_dispatch_free(DskDispatch *dispatch);

DskDispatch        *dsk_dispatch_default (void);

typedef void (*DskDispatchCallback)  (DskFileDescriptor   fd,
                                            unsigned       events,
                                            void          *callback_data);

/* Registering file-descriptors to watch. */
void  dsk_dispatch_watch_fd (DskDispatch *dispatch,
                                    DskFileDescriptor        fd,
                                    unsigned            events,
                                    DskDispatchCallback callback,
                                    void               *callback_data);
void  dsk_dispatch_close_fd (DskDispatch *dispatch,
                                    DskFileDescriptor        fd);
void  dsk_dispatch_fd_closed(DskDispatch *dispatch,
                                    DskFileDescriptor        fd);

/* Timers */
typedef void (*DskDispatchTimerFunc) (DskDispatch *dispatch,
                                            void              *func_data);
DskDispatchTimer *
      dsk_dispatch_add_timer(DskDispatch *dispatch,
                                    unsigned           timeout_secs,
                                    unsigned           timeout_usecs,
                                    DskDispatchTimerFunc func,
                                    void               *func_data);
DskDispatchTimer *
      dsk_dispatch_add_timer_millis
                                   (DskDispatch *dispatch,
                                    unsigned           milliseconds,
                                    DskDispatchTimerFunc func,
                                    void               *func_data);
void  dsk_dispatch_adjust_timer    (DskDispatchTimer *timer,
                                    unsigned          milliseconds);
void  dsk_dispatch_adjust_timer_millis (DskDispatchTimer *timer,
                                        unsigned          milliseconds);
void  dsk_dispatch_remove_timer (DskDispatchTimer *);

/* Idle functions */
typedef void (*DskDispatchIdleFunc)   (DskDispatch *dispatch,
                                             void               *func_data);
DskDispatchIdle *
      dsk_dispatch_add_idle (DskDispatch *dispatch,
                                    DskDispatchIdleFunc func,
                                    void               *func_data);
void  dsk_dispatch_remove_idle (DskDispatchIdle *);

/* --- API for use in standalone application --- */
/* Where you are happy just to run poll(2). */

/* dsk_dispatch_run() 
 * Run one main-loop iteration, using poll(2) (or some system-level event system);
 * 'timeout' is in milliseconds, -1 for no timeout.
 */
void  dsk_dispatch_run      (DskDispatch *dispatch);

#define dsk_main_run_once()  dsk_dispatch_run(dsk_dispatch_default())


/* --- API for those who want to embed a dispatch into their own main-loop --- */
typedef struct {
  DskFileDescriptor fd;
  Dsk_Events events;
} DskFileDescriptorNotify;

typedef struct {
  DskFileDescriptor fd;
  Dsk_Events old_events;
  Dsk_Events events;
} DskFileDescriptorNotifyChange;

void  dsk_dispatch_dispatch (DskDispatch *dispatch,
                                    size_t              n_notifies,
                                    DskFileDescriptorNotify *notifies);
void  dsk_dispatch_clear_changes (DskDispatch *);


struct _DskDispatch
{
  /* changes to the events you are interested in. */
  /* (this handles closed file-descriptors 
     in a manner agreeable to epoll(2) and kqueue(2)) */
  size_t n_changes;
  DskFileDescriptorNotifyChange *changes;

  /* the complete set of events you are interested in. */
  size_t n_notifies_desired;
  DskFileDescriptorNotify *notifies_desired;

  /* number of milliseconds to wait if no events occur */
  dsk_boolean has_timeout;
  unsigned long timeout_secs;
  unsigned timeout_usecs;

  /* true if there is an idle function, in which case polling with
     timeout 0 is appropriate */
  dsk_boolean has_idle;

  unsigned long last_dispatch_secs;
  unsigned last_dispatch_usecs;

  /* private data follows (see RealDispatch structure in .c file) */
};

void dsk_dispatch_destroy_default (void);

#endif
