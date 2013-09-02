#ifndef __WAITTYPE_H__
#define __WAITTYPE_H__

#include <mini-os/list.h>

struct thread;
struct wait_queue
{
    int waiting;
    struct thread *thread;
    MINIOS_STAILQ_ENTRY(struct wait_queue) thread_list;
};

/* TODO - lock required? */
MINIOS_STAILQ_HEAD(wait_queue_head, struct wait_queue);

#define DECLARE_WAIT_QUEUE_HEAD(name) \
    struct wait_queue_head name = MINIOS_STAILQ_HEAD_INITIALIZER(name)

#define __WAIT_QUEUE_HEAD_INITIALIZER(name) MINIOS_STAILQ_HEAD_INITIALIZER(name)

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
