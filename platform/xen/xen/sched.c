/* 
 ****************************************************************************
 * (C) 2005 - Grzegorz Milos - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: sched.c
 *      Author: Grzegorz Milos
 *     Changes: Robert Kaiser
 *              
 *        Date: Aug 2005
 * 
 * Environment: Xen Minimal OS
 * Description: simple scheduler for Mini-Os
 *
 * The scheduler is non-preemptive (cooperative), and schedules according 
 * to Round Robin algorithm.
 *
 ****************************************************************************
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#include <mini-os/os.h>
#include <mini-os/hypervisor.h>
#include <mini-os/time.h>
#include <mini-os/mm.h>
#include <mini-os/types.h>
#include <mini-os/lib.h>
#include <mini-os/sched.h>
#include <mini-os/semaphore.h>

#include <sys/queue.h>

#include <bmk-core/memalloc.h>
#include <bmk-core/string.h>

TAILQ_HEAD(thread_list, thread);

struct thread *idle_thread = NULL;
static struct thread_list exited_threads = TAILQ_HEAD_INITIALIZER(exited_threads);
static struct thread_list thread_list = TAILQ_HEAD_INITIALIZER(thread_list);
static int threads_started;

void inline
print_runqueue(void)
{
	struct thread *th;
	TAILQ_FOREACH(th, &thread_list, thread_list) {
		minios_printk("   Thread \"%s\", runnable=%d\n",
		    th->name, is_runnable(th));
	}
	minios_printk("\n");
}

static void (*scheduler_hook)(void *, void *);

void
switch_threads(struct thread *prev, struct thread *next)
{

	if (scheduler_hook)
		scheduler_hook(prev->cookie, next->cookie);
	arch_switch_threads(prev, next);
}

void
minios_schedule(void)
{
	struct thread *prev, *next, *thread, *tmp;
	unsigned long flags;

	prev = get_current();
	local_irq_save(flags); 

	if (_minios_in_hypervisor_callback) {
		minios_printk("Must not call schedule() from a callback\n");
		BUG();
	}
	if (flags) {
		minios_printk("Must not call schedule() with IRQs disabled\n");
		BUG();
	}

	do {
		s_time_t now = NOW();
		s_time_t min_wakeup_time = now + SECONDS(10);
		next = NULL;
		TAILQ_FOREACH_SAFE(thread, &thread_list, thread_list, tmp) {
			if (!is_runnable(thread)
			    && thread->wakeup_time != 0LL) {
				if (thread->wakeup_time <= now) {
					thread->flags |= THREAD_TIMEDOUT;
					minios_wake(thread);
				} else if (thread->wakeup_time < min_wakeup_time)
					min_wakeup_time = thread->wakeup_time;
			}
			if (is_runnable(thread)) {
				next = thread;
				/* Put this thread on the end of the list */
				TAILQ_REMOVE(&thread_list, thread, thread_list);
				TAILQ_INSERT_TAIL(&thread_list, thread, thread_list);
				break;
			}
		}
		if (next)
			break;

		/*
		 * no runnables.  hlt for a while.
		 */
		block_domain(min_wakeup_time);
		/* handle pending events if any */
		minios_force_evtchn_callback();
	} while(1);

	local_irq_restore(flags);

	if (prev != next) {
		switch_threads(prev, next);
	}

	/* reaper */
	TAILQ_FOREACH_SAFE(thread, &exited_threads, thread_list, tmp) {
		if (thread != prev) {
			TAILQ_REMOVE(&exited_threads, thread, thread_list);
			if ((thread->flags & THREAD_EXTSTACK) == 0)
				minios_free_pages(thread->stack, STACK_SIZE_PAGE_ORDER);
			bmk_memfree(thread);
		}
	}
}

/*
 * Allocate tls and initialize it.
 *
 * XXX: this needs to change in the future so that
 * we put the tcb in the same space instead of having multiple
 * random copies flying around.
 */
extern const char _tdata_start[], _tdata_end[];
extern const char _tbss_start[], _tbss_end[];
static int
allocothertls(struct thread *thread)
{
	const size_t tdatasize = _tdata_end - _tdata_start;
	const size_t tbsssize = _tbss_end - _tbss_start;
	uint8_t *tlsmem;

	tlsmem = bmk_memalloc(tdatasize + tbsssize, 0);

	bmk_memcpy(tlsmem, _tdata_start, tdatasize);
	bmk_memset(tlsmem + tdatasize, 0, tbsssize);

	thread->thr_tp = (uintptr_t)(tlsmem + tdatasize + tbsssize);
	thread->thr_tl = tdatasize + tbsssize;

	return 0;
}

static void
freeothertls(struct thread *thread)
{
	void *mem;

	mem = (void *)(thread->thr_tp);
	bmk_memfree(mem);
}

struct thread *
minios_create_thread(const char *name, void *cookie,
	void (*function)(void *), void *data, void *stack)
{
	struct thread *thread;
	unsigned long flags;
	/* Call architecture specific setup. */
	thread = arch_create_thread(name, function, data, stack);
	/* Not runable, not exited, not sleeping */
	thread->flags = 0;
	thread->wakeup_time = 0LL;
	thread->lwp = NULL;
	thread->cookie = cookie;
	set_runnable(thread);
	allocothertls(thread);
	local_irq_save(flags);
	TAILQ_INSERT_TAIL(&thread_list, thread, thread_list);
	local_irq_restore(flags);
	return thread;
}

struct join_waiter {
	struct thread *jw_thread;
	struct thread *jw_wanted;
	TAILQ_ENTRY(join_waiter) jw_entries;
};
static TAILQ_HEAD(, join_waiter) joinwq = TAILQ_HEAD_INITIALIZER(joinwq);

void
minios_exit_thread(void)
{
	unsigned long flags;
	struct thread *thread = get_current();
	struct join_waiter *jw_iter;

	/* if joinable, gate until we are allowed to exit */
	local_irq_save(flags);
	while (thread->flags & THREAD_MUSTJOIN) {
		thread->flags |= THREAD_JOINED;
		local_irq_restore(flags);

		/* see if the joiner is already there */
		TAILQ_FOREACH(jw_iter, &joinwq, jw_entries) {
			if (jw_iter->jw_wanted == thread) {
				minios_wake(jw_iter->jw_thread);
				break;
			}
		}
		minios_block(thread);
		minios_schedule();
		local_irq_save(flags);
	}

	/* interrupts still disabled ... */

	/* Remove from the thread list */
	TAILQ_REMOVE(&thread_list, thread, thread_list);
	clear_runnable(thread);
	/* Put onto exited list */
	TAILQ_INSERT_HEAD(&exited_threads, thread, thread_list);
	local_irq_restore(flags);

	freeothertls(thread);

	/* Schedule will free the resources */
	for (;;) {
		minios_schedule();
		minios_printk("schedule() returned!  Trying again\n");
	}
}

/* hmm, all of the interfaces here are namespaced "backwards" ... */
void
minios_join_thread(struct thread *joinable)
{
	struct join_waiter jw;
	struct thread *thread = get_current();
	unsigned long flags;

	local_irq_save(flags);
	ASSERT(joinable->flags & THREAD_MUSTJOIN);
	/* wait for exiting thread to hit thread_exit() */
	while (!(joinable->flags & THREAD_JOINED)) {
		local_irq_restore(flags);

		jw.jw_thread = thread;
		jw.jw_wanted = joinable;
		TAILQ_INSERT_TAIL(&joinwq, &jw, jw_entries);
		minios_block(thread);
		minios_schedule();
		TAILQ_REMOVE(&joinwq, &jw, jw_entries);

		local_irq_save(flags);
	}

	/* signal exiting thread that we have seen it and it may now exit */
	ASSERT(joinable->flags & THREAD_JOINED);
	joinable->flags &= ~THREAD_MUSTJOIN;
	local_irq_restore(flags);

	minios_wake(joinable);
}

void
minios_block_timeout(struct thread *thread, uint64_t deadline)
{

	thread->wakeup_time = deadline;
	clear_runnable(thread);
}

void
minios_block(struct thread *thread)
{

	minios_block_timeout(thread, 0);
}

static int
dosleep(s_time_t wakeuptime)
{
	struct thread *thread = get_current();
	int rv;

	thread->wakeup_time = wakeuptime;
	thread->flags &= ~THREAD_TIMEDOUT;
	clear_runnable(thread);
	minios_schedule();

	rv = !!(thread->flags & THREAD_TIMEDOUT);
	thread->flags &= ~THREAD_TIMEDOUT;
	return rv;
}

int
minios_msleep(uint64_t millisecs)
{

	return dosleep(NOW() + MILLISECS(millisecs));
}

int
minios_absmsleep(uint64_t millisecs)
{
	uint32_t secs;
	uint64_t nsecs;

	/* oh the silliness! */
	minios_clock_wall(&secs, &nsecs);
	millisecs -= 1000ULL*(uint64_t)secs + nsecs/(1000ULL*1000);

	return dosleep(MILLISECS(millisecs) + NOW());
}

void
minios_wake(struct thread *thread)
{
	thread->wakeup_time = 0LL;
	set_runnable(thread);
}

void
idle_thread_fn(void *unused)
{
	threads_started = 1;
	for (;;) {
		minios_block(get_current());
		minios_schedule();
	}
}

void
init_sched(void)
{
	minios_printk("Initialising scheduler\n");

	idle_thread = minios_create_thread("Idle", NULL, idle_thread_fn, NULL, NULL);
}

void
minios_set_sched_hook(void (*f)(void *, void *))
{

	scheduler_hook = f;
}

struct thread *
minios_init_mainlwp(void *cookie)
{
	struct thread *current = get_current();

	current->cookie = cookie;
	allocothertls(current);
	return current;
}
