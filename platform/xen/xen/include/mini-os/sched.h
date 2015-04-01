#ifndef __MINIOS_SCHED_H__
#define __MINIOS_SCHED_H__

#include <mini-os/time.h>
#include <mini-os/machine/sched.h>

#include <sys/queue.h>

#include <bmk-core/sched.h>

extern struct bmk_tcb *idle_tcb;
void idle_thread_fn(void *unused);

void switch_threads(struct thread *prev, struct thread *next);
 
    /* Architecture specific setup of thread creation. */
void arch_create_thread(void *, struct bmk_tcb *,
	void (*function)(void *), void *data, void *stack);

void init_sched(void);
void run_idle_thread(void);
struct thread* minios_create_thread(const char *name, void *cookie,
			     int joinable,
			     void (*f)(void *), void *data, void *stack);
void minios_exit_thread(void) __attribute__((noreturn));
void minios_join_thread(struct thread *);
void minios_set_sched_hook(void (*hook)(void *, void *));
struct thread *minios_init_mainlwp(void *cookie);
void minios_schedule(void);

void minios_wake(struct thread *thread);
void minios_block_timeout(struct thread *thread, uint64_t);
void minios_block(struct thread *thread);
int minios_msleep(uint64_t millisecs);
int minios_absmsleep(uint64_t millisecs);

const char *minios_threadname(struct thread *);
int *minios_sched_geterrno(void);

void minios_sched_settls(struct thread *, unsigned int, void *);
void *minios_sched_gettls(struct thread *, unsigned int);

#endif /* __MINIOS_SCHED_H__ */
