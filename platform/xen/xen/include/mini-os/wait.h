#ifndef __MINIOS_WAIT_H__
#define __MINIOS_WAIT_H__

#include <mini-os/os.h>
#include <mini-os/waittypes.h>
#include <mini-os/time.h>

#include <sys/queue.h>

#define DEFINE_WAIT(name)                          \
struct wait_queue name = {                         \
    .thread       = bmk_current,	           \
    .waiting      = 0,                             \
}


static inline void minios_init_waitqueue_head(struct wait_queue_head *h)
{
    STAILQ_INIT(h);
}

static inline void minios_init_waitqueue_entry(struct wait_queue *q, struct bmk_thread *thread)
{
    q->thread = thread;
    q->waiting = 0;
}

static inline void minios_add_wait_queue(struct wait_queue_head *h, struct wait_queue *q)
{
    if (!q->waiting) {
        STAILQ_INSERT_HEAD(h, q, thread_list);
        q->waiting = 1;
    }
}

static inline void minios_remove_wait_queue(struct wait_queue_head *h, struct wait_queue *q)
{
    if (q->waiting) {
        STAILQ_REMOVE(h, q, wait_queue, thread_list);
        q->waiting = 0;
    }
}

static inline void minios_wake_up(struct wait_queue_head *head)
{
    unsigned long flags;
    struct wait_queue *curr, *tmp;
    local_irq_save(flags);
    STAILQ_FOREACH_SAFE(curr, head, thread_list, tmp)
         bmk_sched_wake(curr->thread);
    local_irq_restore(flags);
}

#define minios_add_waiter(w, wq) do {  \
    unsigned long flags;        \
    local_irq_save(flags);      \
    minios_add_wait_queue(&wq, &w);    \
    bmk_sched_blockprepare();      \
    local_irq_restore(flags);   \
} while (0)

#define minios_remove_waiter(w, wq) do {  \
    unsigned long flags;           \
    local_irq_save(flags);         \
    minios_remove_wait_queue(&wq, &w);    \
    local_irq_restore(flags);      \
} while (0)

#define minios_wait_event_deadline(wq, condition, deadline) do {       \
    unsigned long flags;                                        \
    DEFINE_WAIT(__wait);                                        \
    for(;;)                                                     \
    {                                                           \
        if(condition)                                           \
            break;                                              \
        /* protect the list */                                  \
        local_irq_save(flags);                                  \
        minios_add_wait_queue(&wq, &__wait);                           \
        local_irq_restore(flags);                               \
        if (bmk_sched_nanosleep_abstime(deadline))		\
	    break;						\
    }                                                           \
    local_irq_save(flags);                                      \
    /* need to wake up */                                       \
    minios_remove_wait_queue(&wq, &__wait);                            \
    local_irq_restore(flags);                                   \
} while(0) 

#define minios_wait_event(wq, condition) minios_wait_event_deadline(wq, condition, BMK_SCHED_BLOCK_INFTIME)



#endif /* __MINIOS_WAIT_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
