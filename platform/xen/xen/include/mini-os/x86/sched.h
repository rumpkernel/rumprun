
#ifndef __ARCH_SCHED_H__
#define __ARCH_SCHED_H__

#include <mini-os/machine/limits.h>

#include <bmk-core/sched.h>

static inline struct bmk_thread *get_current(void)
{
    struct bmk_thread **current;

    current = (void *)((unsigned long)&current & ~(__STACK_SIZE-1));
    return *current;
};

extern void _minios_entry_arch_switch_threads(struct bmk_tcb *prevctx, struct bmk_tcb *nextctx);

#define arch_switch_threads(prev,next) _minios_entry_arch_switch_threads(prev, next)

#endif /* __ARCH_SCHED_H__ */
