/*-
 * Copyright (c) 2007-2015 Antti Kantee.  All Rights Reserved.
 * Copyright (c) 2014 Justin Cormack.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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

#define assert(x) ASSERT(x)

#include <sys/queue.h>

#include <bmk-core/bmk_ops.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/string.h>
#include <bmk-core/sched.h>

#define TLS_COUNT 2
#define NAME_MAXLEN 16

#define THREAD_RUNNABLE	0x01
#define THREAD_MUSTJOIN	0x02
#define THREAD_JOINED	0x04
#define THREAD_EXTSTACK	0x08
#define THREAD_TIMEDOUT	0x10

struct bmk_thread {
	char bt_name[NAME_MAXLEN];

	void *bt_tls[TLS_COUNT];

	int64_t bt_wakeup_time;

	int bt_flags;
	int bt_errno;

	void *bt_stackbase;

	void *bt_cookie;

	/* MD thread control block */
	struct bmk_tcb bt_tcb;

	TAILQ_ENTRY(bmk_thread) bt_entries;
};

static struct bmk_thread *idle_thread = NULL;
struct bmk_tcb *idle_tcb;

static TAILQ_HEAD(, bmk_thread) zombies = TAILQ_HEAD_INITIALIZER(zombies);
static TAILQ_HEAD(, bmk_thread) threads = TAILQ_HEAD_INITIALIZER(threads);

static void (*scheduler_hook)(void *, void *);

static int
is_runnable(struct bmk_thread *thread)
{

	return thread->bt_flags & THREAD_RUNNABLE;
}

static void
set_runnable(struct bmk_thread *thread)
{

	thread->bt_flags |= THREAD_RUNNABLE;
}

static void
clear_runnable(struct bmk_thread *thread)
{

	thread->bt_flags &= ~THREAD_RUNNABLE;
}

static void
stackalloc(void **stack, unsigned long *ss)
{

	*stack = (void *)minios_alloc_pages(STACK_SIZE_PAGE_ORDER);
	*ss = STACK_SIZE;
}

static void
stackfree(struct bmk_thread *thread)
{

	minios_free_pages(thread->bt_stackbase, STACK_SIZE_PAGE_ORDER);
}

void
sched_switch(struct bmk_thread *prev, struct bmk_thread *next)
{

	if (scheduler_hook)
		scheduler_hook(prev->bt_cookie, next->bt_cookie);
	arch_switch_threads(&prev->bt_tcb, &next->bt_tcb);
}

void
minios_schedule(void)
{
	struct bmk_thread *prev, *next, *thread, *tmp;
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

	/* could do time management a bit better here */
	do {
		s_time_t tm = NOW();
		s_time_t wakeup = tm + SECONDS(10);
		next = NULL;
		TAILQ_FOREACH_SAFE(thread, &threads, bt_entries, tmp) {
			if (!is_runnable(thread)
			    && thread->bt_wakeup_time >= 0) {
				if (thread->bt_wakeup_time <= tm) {
					thread->bt_flags |= THREAD_TIMEDOUT;
					minios_wake(thread);
				} else if (thread->bt_wakeup_time < wakeup)
					wakeup = thread->bt_wakeup_time;
			}
			if (is_runnable(thread)) {
				next = thread;
				/* Put this thread on the end of the list */
				TAILQ_REMOVE(&threads, thread, bt_entries);
				TAILQ_INSERT_TAIL(&threads, thread, bt_entries);
				break;
			}
		}
		if (next)
			break;
		/*
		 * no runnables.  hlt for a while.
		 */
		block_domain(wakeup);
		/* handle pending events if any */
		minios_force_evtchn_callback();
	} while(1);

	local_irq_restore(flags);

	if (prev != next) {
		sched_switch(prev, next);
	}

	/* reaper */
	TAILQ_FOREACH_SAFE(thread, &zombies, bt_entries, tmp) {
		if (thread != prev) {
			TAILQ_REMOVE(&zombies, thread, bt_entries);
			if ((thread->bt_flags & THREAD_EXTSTACK) == 0)
				stackfree(thread);
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
allocothertls(struct bmk_thread *thread)
{
	const size_t tdatasize = _tdata_end - _tdata_start;
	const size_t tbsssize = _tbss_end - _tbss_start;
	struct bmk_tcb *tcb = &thread->bt_tcb;
	uint8_t *tlsmem;

	tlsmem = bmk_memalloc(tdatasize + tbsssize, 0);

	bmk_memcpy(tlsmem, _tdata_start, tdatasize);
	bmk_memset(tlsmem + tdatasize, 0, tbsssize);

	tcb->btcb_tp = (unsigned long)(tlsmem + tdatasize + tbsssize);
	tcb->btcb_tpsize = tdatasize + tbsssize;

	return 0;
}

static void
freeothertls(struct bmk_thread *thread)
{
	void *mem;

	mem = (void *)(thread->bt_tcb.btcb_tp-thread->bt_tcb.btcb_tpsize);
	bmk_memfree(mem);
}

struct bmk_thread *
minios_create_thread(const char *name, void *cookie, int joinable,
	void (*f)(void *), void *data,
	void *stack_base, unsigned long stack_size)
{
	struct bmk_thread *thread;
	unsigned long flags;

	thread = bmk_xmalloc(sizeof(*thread));
	bmk_memset(thread, 0, sizeof(*thread));
	bmk_strncpy(thread->bt_name, name, sizeof(thread->bt_name)-1);

	if (!stack_base) {
		assert(stack_size == 0);
		stackalloc(&stack_base, &stack_size);
	} else {
		thread->bt_flags = THREAD_EXTSTACK;
	}
	thread->bt_stackbase = stack_base;
	if (joinable)
		thread->bt_flags |= THREAD_MUSTJOIN;

	arch_create_thread(thread, &thread->bt_tcb, f, data,
	    stack_base, stack_size);

	thread->bt_cookie = cookie;

	thread->bt_wakeup_time = -1;

	local_irq_save(flags);
	TAILQ_INSERT_TAIL(&threads, thread, bt_entries);
	local_irq_restore(flags);

	allocothertls(thread);
	set_runnable(thread);

	return thread;
}

struct join_waiter {
	struct bmk_thread *jw_thread;
	struct bmk_thread *jw_wanted;
	TAILQ_ENTRY(join_waiter) jw_entries;
};
static TAILQ_HEAD(, join_waiter) joinwq = TAILQ_HEAD_INITIALIZER(joinwq);

void
minios_exit_thread(void)
{
	unsigned long flags;
	struct bmk_thread *thread = get_current();
	struct join_waiter *jw_iter;

	/* if joinable, gate until we are allowed to exit */
	local_irq_save(flags);
	while (thread->bt_flags & THREAD_MUSTJOIN) {
		thread->bt_flags |= THREAD_JOINED;
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
	freeothertls(thread);

	/* Remove from the thread list */
	TAILQ_REMOVE(&threads, thread, bt_entries);
	clear_runnable(thread);
	/* Put onto exited list */
	TAILQ_INSERT_HEAD(&zombies, thread, bt_entries);
	local_irq_restore(flags);

	/* bye */
	minios_schedule();
	bmk_ops->bmk_halt("schedule() returned for a dead thread!\n");
}

/* hmm, all of the interfaces here are namespaced "backwards" ... */
void
minios_join_thread(struct bmk_thread *joinable)
{
	struct join_waiter jw;
	struct bmk_thread *thread = get_current();
	unsigned long flags;

	assert(joinable->bt_flags & THREAD_MUSTJOIN);

	local_irq_save(flags);
	/* wait for exiting thread to hit thread_exit() */
	while ((joinable->bt_flags & THREAD_JOINED) == 0) {
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
	assert(joinable->bt_flags & THREAD_JOINED);
	joinable->bt_flags &= ~THREAD_MUSTJOIN;
	local_irq_restore(flags);

	minios_wake(joinable);
}

void
minios_block_timeout(struct bmk_thread *thread, uint64_t deadline)
{

	thread->bt_wakeup_time = deadline;
	clear_runnable(thread);
}

void
minios_block(struct bmk_thread *thread)
{

	minios_block_timeout(thread, -1);
}

static int
dosleep(s_time_t wakeuptime)
{
	struct bmk_thread *thread = get_current();
	int rv;

	thread->bt_wakeup_time = wakeuptime;
	thread->bt_flags &= ~THREAD_TIMEDOUT;
	clear_runnable(thread);
	minios_schedule();

	rv = !!(thread->bt_flags & THREAD_TIMEDOUT);
	thread->bt_flags &= ~THREAD_TIMEDOUT;
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
minios_wake(struct bmk_thread *thread)
{

	thread->bt_wakeup_time = -1;
	set_runnable(thread);
}

void
idle_thread_fn(void *unused)
{

	for (;;) {
		minios_block(get_current());
		minios_schedule();
	}
}

void
init_sched(void)
{
	minios_printk("Initialising scheduler\n");

	idle_thread = minios_create_thread("Idle", NULL, 0,
	    idle_thread_fn, NULL, NULL, 0);
	idle_tcb = &idle_thread->bt_tcb;
}

void
minios_set_sched_hook(void (*f)(void *, void *))
{

	scheduler_hook = f;
}

struct bmk_thread *
minios_init_mainlwp(void *cookie)
{
	struct bmk_thread *current = get_current();

	current->bt_cookie = cookie;
	allocothertls(current);
	return current;
}

const char *
minios_threadname(struct bmk_thread *thread)
{

	return thread->bt_name;
}

int *
minios_sched_geterrno(void)
{
	struct bmk_thread *thread = get_current();

	return &thread->bt_errno;
}

void
minios_sched_settls(struct bmk_thread *thread, unsigned int which, void *value)
{

	if (which >= TLS_COUNT) {
		bmk_ops->bmk_halt("out of bmk sched tls space");
	}
	thread->bt_tls[which] = value;
}

void *
minios_sched_gettls(struct bmk_thread *thread, unsigned int which)
{

	if (which >= TLS_COUNT) {
		bmk_ops->bmk_halt("out of bmk sched tls space");
	}
	return thread->bt_tls[which];
}
