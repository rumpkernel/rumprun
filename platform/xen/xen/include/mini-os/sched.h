#ifndef __MINIOS_SCHED_H__
#define __MINIOS_SCHED_H__

#include <mini-os/types.h>

#include <mini-os/machine/sched.h>

#include <bmk-core/sched.h>

extern struct bmk_tcb *idle_tcb;
void idle_thread_fn(void *unused);

void run_idle_thread(void);

#endif /* __MINIOS_SCHED_H__ */
