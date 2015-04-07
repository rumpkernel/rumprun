
#ifndef __ARCH_SCHED_H__
#define __ARCH_SCHED_H__

#include <mini-os/machine/limits.h>

#include <bmk-core/sched.h>

static inline struct bmk_thread *arch_sched_current(void)
{
    struct bmk_thread **current;

    current = (void *)((unsigned long)&current & ~(__STACK_SIZE-1));
    return *current;
};

#endif /* __ARCH_SCHED_H__ */
