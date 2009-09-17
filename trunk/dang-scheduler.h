
typedef struct _DangScheduler DangScheduler;
struct _DangScheduler
{
  unsigned ref_count;
  DangSchedulerQueue *run_queue_start, *run_queue_end;
};

void        dang_scheduler_push (DangScheduler *scheduler,
                                 DangThread    *thread);
DangThread *dang_scheduler_pop  (DangScheduler *scheduler);


