#include <bmk/bmk_types.h>

#include <bmk-core/sched.h>

#define BMK_THREAD_STACKSIZE (1<<16)

bmk_time_t bmk_cpu_clock_now(void);

void	lwp_rumprun_init(void);
