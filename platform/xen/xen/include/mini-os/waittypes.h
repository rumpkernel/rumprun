#ifndef __MINIOS_WAITTYPE_H__
#define __MINIOS_WAITTYPE_H__

#include <bmk-core/sched.h>
#include <bmk-core/queue.h>

struct wait_queue
{
    int waiting;
    struct bmk_thread *thread;
    STAILQ_ENTRY(wait_queue) thread_list;
};

/* TODO - lock required? */
STAILQ_HEAD(wait_queue_head, wait_queue);

#define DECLARE_WAIT_QUEUE_HEAD(name) \
    struct wait_queue_head name = STAILQ_HEAD_INITIALIZER(name)

#define __WAIT_QUEUE_HEAD_INITIALIZER(name) STAILQ_HEAD_INITIALIZER(name)

#endif

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
