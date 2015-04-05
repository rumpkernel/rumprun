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

#include <bmk/types.h>
#include <bmk/queue.h>

#include <bmk/kernel.h>
#include <bmk/sched.h>

#include <bmk-core/core.h>
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

static TAILQ_HEAD(, bmk_thread) zombies = TAILQ_HEAD_INITIALIZER(zombies);
static TAILQ_HEAD(, bmk_thread) threads = TAILQ_HEAD_INITIALIZER(threads);

static void (*scheduler_hook)(void *, void *);

static struct bmk_thread *current_thread = NULL;
struct bmk_thread *
bmk_sched_current(void)
{

	return current_thread;
}

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

	*stack = bmk_xmalloc(BMK_THREAD_STACKSIZE);
	*ss = BMK_THREAD_STACKSIZE;
}

static void
stackfree(struct bmk_thread *thread)
{

	bmk_memfree(thread->bt_stackbase);
}

static void
sched_switch(struct bmk_thread *prev, struct bmk_thread *next)
{

	if (scheduler_hook)
		scheduler_hook(prev->bt_cookie, next->bt_cookie);
	current_thread = next;
	bmk_cpu_sched_switch(&prev->bt_tcb, &next->bt_tcb);
}

void
bmk_sched(void)
{
	struct bmk_thread *prev, *next, *thread, *tmp;
	bmk_time_t tm, wakeup;

	prev = bmk_sched_current();

	/* could do time management a bit better here */
	do {
		tm = bmk_clock_monotonic();
		next = NULL;
		TAILQ_FOREACH_SAFE(thread, &threads, bt_entries, tmp) {
			if (!is_runnable(thread)
			    && thread->bt_wakeup_time >= 0) {
				if (thread->bt_wakeup_time <= tm) {
					thread->bt_flags |= THREAD_TIMEDOUT;
					bmk_sched_wake(thread);
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
		bmk_cpu_nanohlt();
	} while (1);

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
bmk_sched_create(const char *name, void *cookie, int joinable,
	void (*f)(void *), void *data,
	void *stack_base, unsigned long stack_size)
{
	struct bmk_thread *thread;

	thread = bmk_xmalloc(sizeof(*thread));
	bmk_strncpy(thread->bt_name, name, sizeof(thread->bt_name)-1);

	if (!stack_base) {
		bmk_assert(stack_size == 0);
		stackalloc(&stack_base, &stack_size);
	} else {
		thread->bt_flags = THREAD_EXTSTACK;
	}
	thread->bt_stackbase = stack_base;
	if (joinable)
		thread->bt_flags |= THREAD_MUSTJOIN;

	bmk_cpu_sched_create(&thread->bt_tcb, f, data, stack_base, stack_size);

	thread->bt_cookie = cookie;

	thread->bt_wakeup_time = -1;

	set_runnable(thread);
	TAILQ_INSERT_HEAD(&threads, thread, bt_entries);

	allocothertls(thread);

	return thread;
}

struct join_waiter {
	struct bmk_thread *jw_thread;
	struct bmk_thread *jw_wanted;
	TAILQ_ENTRY(join_waiter) jw_entries;
};
static TAILQ_HEAD(, join_waiter) joinwq = TAILQ_HEAD_INITIALIZER(joinwq);

void
bmk_sched_exit(void)
{
	struct bmk_thread *thread = bmk_sched_current();
	struct join_waiter *jw_iter;

	/* if joinable, gate until we are allowed to exit */
	while (thread->bt_flags & THREAD_MUSTJOIN) {
		thread->bt_flags |= THREAD_JOINED;

		/* see if the joiner is already there */
		TAILQ_FOREACH(jw_iter, &joinwq, jw_entries) {
			if (jw_iter->jw_wanted == thread) {
				bmk_sched_wake(jw_iter->jw_thread);
				break;
			}
		}
		bmk_sched_block(thread);
		bmk_sched();
	}
	freeothertls(thread);

	/* Remove from the thread list */
	TAILQ_REMOVE(&threads, thread, bt_entries);
	clear_runnable(thread);
	/* Put onto exited list */
	TAILQ_INSERT_HEAD(&zombies, thread, bt_entries);

	/* bye */
	bmk_sched();
	bmk_ops->bmk_halt("schedule() returned for a dead thread!\n");
}

void
bmk_sched_join(struct bmk_thread *joinable)
{
	struct join_waiter jw;
	struct bmk_thread *thread = bmk_sched_current();

	bmk_assert(joinable->bt_flags & THREAD_MUSTJOIN);

	/* wait for exiting thread to hit thread_exit() */
	while ((joinable->bt_flags & THREAD_JOINED) == 0) {
		jw.jw_thread = thread;
		jw.jw_wanted = joinable;
		TAILQ_INSERT_TAIL(&joinwq, &jw, jw_entries);
		bmk_sched_block(thread);
		bmk_sched();
		TAILQ_REMOVE(&joinwq, &jw, jw_entries);
	}

	/* signal exiting thread that we have seen it and it may now exit */
	bmk_assert(joinable->bt_flags & THREAD_JOINED);
	joinable->bt_flags &= ~THREAD_MUSTJOIN;

	bmk_sched_wake(joinable);
}

void
bmk_sched_block_timeout(struct bmk_thread *thread, bmk_time_t deadline)
{

	thread->bt_wakeup_time = deadline;
	clear_runnable(thread);
}

void
bmk_sched_block(struct bmk_thread *thread)
{

	bmk_sched_block_timeout(thread, -1);
}

int
bmk_sched_nanosleep_abstime(bmk_time_t nsec)
{
	struct bmk_thread *thread = bmk_sched_current();
	int rv;

	thread->bt_flags &= ~THREAD_TIMEDOUT;
	thread->bt_wakeup_time = nsec;
	clear_runnable(thread);
	bmk_sched();

	rv = !!(thread->bt_flags & THREAD_TIMEDOUT);
	thread->bt_flags &= ~THREAD_TIMEDOUT;
	return rv;
}

int
bmk_sched_nanosleep(bmk_time_t nsec)
{

	return bmk_sched_nanosleep_abstime(nsec + bmk_clock_monotonic());
}

void
bmk_sched_setwakeup(struct bmk_thread *thread, bmk_time_t abstime)
{

	thread->bt_wakeup_time = abstime;
}

void
bmk_sched_wake(struct bmk_thread *thread)
{

	thread->bt_wakeup_time = -1;
	set_runnable(thread);
}

static struct bmk_thread init_thread;

void
bmk_sched_init(void)
{
	struct bmk_thread *thread = &init_thread;

	bmk_strncpy(thread->bt_name, "init", sizeof(thread->bt_name)-1);
	thread->bt_flags = 0;
	thread->bt_wakeup_time = -1;
	set_runnable(thread);
	TAILQ_INSERT_TAIL(&threads, thread, bt_entries);
	current_thread = thread;
}

void
bmk_sched_set_hook(void (*f)(void *, void *))
{

	scheduler_hook = f;
}

struct bmk_thread *
bmk_sched_init_mainlwp(void *cookie)
{
	struct bmk_thread *current = bmk_sched_current();

	current->bt_cookie = cookie;
	allocothertls(current);
	return current;
}

int *
bmk_sched_geterrno(void)
{
	struct bmk_thread *thread = bmk_sched_current();

	return &thread->bt_errno;
}

void
bmk_sched_settls(struct bmk_thread *thread, unsigned int which, void *value)
{

	if (which >= TLS_COUNT) {
		bmk_ops->bmk_halt("out of bmk sched tls space");
	}
	thread->bt_tls[which] = value;
}

void *
bmk_sched_gettls(struct bmk_thread *thread, unsigned int which)
{

	if (which >= TLS_COUNT) {
		bmk_ops->bmk_halt("out of bmk sched tls space");
	}
	return thread->bt_tls[which];
}
