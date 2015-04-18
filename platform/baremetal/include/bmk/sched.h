#include <bmk-core/types.h>
#include <bmk-core/sched.h>

#define BMK_THREAD_STACK_PAGE_ORDER 4
#define BMK_THREAD_STACKSIZE ((1<<BMK_THREAD_STACK_PAGE_ORDER) * PAGE_SIZE)

bmk_time_t bmk_cpu_clock_now(void);
