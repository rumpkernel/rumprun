#ifndef __MINIOS_SCHED_H__
#define __MINIOS_SCHED_H__

#include <mini-os/types.h>

typedef int64_t bmk_time_t;

#include <mini-os/machine/sched.h>

#include <bmk-core/sched.h>

extern struct bmk_tcb *idle_tcb;
void idle_thread_fn(void *unused);

void run_idle_thread(void);

void arch_create_thread(void *thread, struct bmk_tcb *,
			void (*function)(void *), void *data,
			void *stack_base, unsigned long stack_size);

const char * minios_threadname(struct bmk_thread *thread);

#endif /* __MINIOS_SCHED_H__ */
