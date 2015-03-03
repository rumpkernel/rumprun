#ifndef __MINIOS_SCHED_H__
#define __MINIOS_SCHED_H__

#include <mini-os/time.h>
#include <mini-os/machine/sched.h>

#include <sys/queue.h>

struct thread {
    const char *name;
    char *stack;
    size_t stack_size;
    struct thread_md md;
    TAILQ_ENTRY(thread) thread_list;
    uint32_t flags;
    s_time_t wakeup_time;
    int threrrno;
    void *lwp;
    void *cookie;
};

extern struct thread *idle_thread;
void idle_thread_fn(void *unused);

#define RUNNABLE_FLAG	0x00000001
#define THREAD_MUSTJOIN	0x00000002
#define THREAD_JOINED	0x00000004
#define THREAD_EXTSTACK	0x00000008
#define THREAD_TIMEDOUT	0x00000010

#define is_runnable(_thread)    (_thread->flags & RUNNABLE_FLAG)
#define set_runnable(_thread)   (_thread->flags |= RUNNABLE_FLAG)
#define clear_runnable(_thread) (_thread->flags &= ~RUNNABLE_FLAG)

void switch_threads(struct thread *prev, struct thread *next);
 
    /* Architecture specific setup of thread creation. */
struct thread* arch_create_thread(const char *name, void (*function)(void *),
                                  void *data, void *stack);

void init_sched(void);
void run_idle_thread(void);
struct thread* minios_create_thread(const char *name, void *cookie,
			     void (*f)(void *), void *data, void *stack);
void minios_exit_thread(void) __attribute__((noreturn));
void minios_join_thread(struct thread *);
void minios_set_sched_hook(void (*hook)(void *, void *));
struct thread *minios_init_mainlwp(void *cookie);
void minios_schedule(void);

void minios_wake(struct thread *thread);
void minios_block(struct thread *thread);
int minios_msleep(uint64_t millisecs);
int minios_absmsleep(uint64_t millisecs);

#endif /* __MINIOS_SCHED_H__ */
