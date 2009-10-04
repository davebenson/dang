
/* --- DangThread: a thread of execution --- */
typedef enum
{
  DANG_THREAD_STATUS_NOT_STARTED,
  DANG_THREAD_STATUS_RUNNING,
  DANG_THREAD_STATUS_YIELDED,
  DANG_THREAD_STATUS_DONE,
  DANG_THREAD_STATUS_THREW,
  DANG_THREAD_STATUS_CANCELLED
} DangThreadStatus;
const char *dang_thread_status_name (DangThreadStatus status);

struct _DangThreadStackFrame
{
  DangThreadStackFrame *caller;
  DangFunction *function;
  DangStep *ip;
};

typedef DangDestroyNotify DangThreadYieldCancelFunc;
typedef void (*DangThreadDoneFunc) (DangThread *thread,
                                    void *done_func_data);

typedef struct _DangThreadCatchGuard DangThreadCatchGuard;
struct _DangThreadCatchGuard
{
  DangThreadStackFrame *stack_frame;
  DangCatchBlock *catch_block;
  DangThreadCatchGuard *parent;
};

struct _DangThread
{
  DangThreadStatus status;
  DangThreadStackFrame *stack_frame;
  DangThreadCatchGuard *catch_guards;
  unsigned ref_count;


  /* only should be used if state==DONE */
  DangThreadStackFrame *rv_frame;
  DangFunction *rv_function;

  union {
    struct {
      /* if the thread ran a command that caused it to yield,
         then it must provide cancellation functions.
         If NULL is supplied then we assume no cleanup is needed--
         which implies that you will handle cancelling the callback yourself
         if the thread is destroyed. */
      DangThreadYieldCancelFunc yield_cancel_func;
      void *yield_cancel_func_data;

      /* Function that may be set by the caller of dang_thread_run()
         to receive notification that the function finally returned or
         threw an exception. */
      DangThreadDoneFunc done_func;
      void *done_func_data;
    } yield;
    struct {
      void *value;
      DangValueType *type;
    } threw;
  } info;
};

/* You must collect the return-value yourself,
   from rv_frame */
DangThread   *dang_thread_new  (DangFunction *function,
                                unsigned      n_arguments,
                                void        **arguments);
void          dang_thread_run  (DangThread   *thread);
void          dang_thread_unref(DangThread   *thread);
DangThread   *dang_thread_ref  (DangThread   *thread);
void          dang_thread_cancel(DangThread   *thread);

void          dang_thread_throw(DangThread   *thread,
                                DangValueType *type,
                                void          *value);
void          dang_thread_throw_error(DangThread   *thread,
                                      DangError    *error);
/* TODO: revise this api to allow the DangThreadCatch to lie in 'stack_frame' */
void          dang_thread_push_catch_guard (DangThread *thread,
                                            DangThreadStackFrame *stack_frame,
                                            DangCatchBlock *catch_block);
void          dang_thread_pop_catch_guard  (DangThread *thread);


 
void dang_thread_throw_null_pointer_exception (DangThread *);
void dang_thread_throw_array_bounds_exception (DangThread *);
char *dang_thread_get_backtrace (DangThread *);

/* useful from various "return" implementations */
void dang_thread_pop_frame (DangThread *thread);
