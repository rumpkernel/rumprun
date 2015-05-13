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

#include <bmk-core/core.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <bmk-core/queue.h>
#include <bmk-core/string.h>
#include <bmk-core/sched.h>

#define TLS_COUNT 2
#define NAME_MAXLEN 16

#define THREAD_RUNNABLE	0x01
#define THREAD_MUSTJOIN	0x02
#define THREAD_JOINED	0x04
#define THREAD_EXTSTACK	0x08
#define THREAD_TIMEDOUT	0x10
#define THREAD_DEAD	0x20

extern const char _tdata_start[], _tdata_end[];
extern const char _tbss_start[], _tbss_end[];
#define TDATASIZE (_tdata_end - _tdata_start)
#define TBSSSIZE (_tbss_end - _tbss_start)
#define TCBOFFSET \
    (((TDATASIZE + TBSSSIZE + sizeof(void *)-1)/sizeof(void *))*sizeof(void *))
#define TLSAREASIZE (TCBOFFSET + BMK_TLS_EXTRA)

struct bmk_thread {
	char bt_name[NAME_MAXLEN];

	void *bt_tls[TLS_COUNT];

	bmk_time_t bt_wakeup_time;

	int bt_flags;
	int bt_errno;

	void *bt_stackbase;

	void *bt_cookie;

	/* MD thread control block */
	struct bmk_tcb bt_tcb;

	TAILQ_ENTRY(bmk_thread) bt_entries;
};
__thread struct bmk_thread *bmk_current;

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

	*stack = bmk_platform_allocpg2(bmk_stackpageorder);
	*ss = bmk_stacksize;
}

static void
stackfree(struct bmk_thread *thread)
{

	bmk_platform_freepg2(thread->bt_stackbase, bmk_stackpageorder);
}

static void
print_threadinfo(struct bmk_thread *thread)
{

	bmk_printf("thread \"%s\" at %p, flags 0x%x\n",
	    thread->bt_name, thread, thread->bt_flags);
}

static void
sched_switch(struct bmk_thread *prev, struct bmk_thread *next)
{

	if (scheduler_hook)
		scheduler_hook(prev->bt_cookie, next->bt_cookie);
	bmk_platform_cpu_sched_settls(&next->bt_tcb);
	bmk_cpu_sched_switch(&prev->bt_tcb, &next->bt_tcb);
}

void
bmk_sched_dumpqueue(void)
{
	struct bmk_thread *thr;

	bmk_printf("BEGIN schedqueue dump\n");
	TAILQ_FOREACH(thr, &threads, bt_entries) {
		print_threadinfo(thr);
	}
	bmk_printf("END schedqueue dump\n");
}

void
bmk_sched(void)
{
	struct bmk_thread *prev, *next, *thread;
	unsigned long flags;

	prev = bmk_current;
	flags = bmk_platform_splhigh();

#if 0
	/* XXX */
	if (_minios_in_hypervisor_callback) {
		minios_printk("Must not call schedule() from a callback\n");
		BUG();
	}
#endif

	if (flags) {
		bmk_platform_halt("Must not call sched() with IRQs disabled\n");
	}

	/* could do time management a bit better here */
	for (;;) {
		bmk_time_t tm, wakeup;

		/* block domain for max 1s */
		tm = bmk_platform_clock_monotonic();
		wakeup = tm + 1*1000*1000*1000ULL;

		next = NULL;
		TAILQ_FOREACH(thread, &threads, bt_entries) {
			if (!is_runnable(thread)
			    && thread->bt_wakeup_time
			      != BMK_SCHED_BLOCK_INFTIME) {
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

		/* sleep for a while */
		bmk_platform_block(wakeup);
	}

	bmk_platform_splx(flags);

	bmk_assert((next->bt_flags & THREAD_DEAD) == 0);

	if (prev != next) {
		sched_switch(prev, next);
	}

	/*
	 * Reaper.  This always runs in the context of the first "non-virgin"
	 * thread that was scheduled after the current thread decided to exit.
	 */
	while ((thread = TAILQ_FIRST(&zombies)) != NULL) {
		TAILQ_REMOVE(&zombies, thread, bt_entries);
		if ((thread->bt_flags & THREAD_EXTSTACK) == 0)
			stackfree(thread);
		bmk_memfree(thread);
	}
}

/*
 * Allocate tls and initialize it.
 * NOTE: does not initialize tcb, see inittcb().
 */
void *
bmk_sched_tls_alloc(void)
{
	char *tlsmem;

	tlsmem = bmk_memalloc(TLSAREASIZE, 0);
	bmk_memcpy(tlsmem, _tdata_start, TDATASIZE);
	bmk_memset(tlsmem + TDATASIZE, 0, TBSSSIZE);

	return tlsmem + TCBOFFSET;
}

/*
 * Free tls
 */
void
bmk_sched_tls_free(void *mem)
{

	mem = (void *)((unsigned long)mem - TCBOFFSET);
	bmk_memfree(mem);
}

void *
bmk_sched_gettcb(void)
{

	return (void *)bmk_current->bt_tcb.btcb_tp;
}

static void
inittcb(struct bmk_tcb *tcb, void *tlsarea, unsigned long tlssize)
{

#if 0
	/* TCB initialization for Variant I */
	/* TODO */
#else
	/* TCB initialization for Variant II */
	*(void **)tlsarea = tlsarea;
	tcb->btcb_tp = (unsigned long)tlsarea;
	tcb->btcb_tpsize = tlssize;
#endif
}

static long bmk_curoff;
static void
initcurrent(void *tcb, struct bmk_thread *value)
{
	struct bmk_thread **dst = (void *)((unsigned long)tcb + bmk_curoff);

	*dst = value;
}

struct bmk_thread *
bmk_sched_create_withtls(const char *name, void *cookie, int joinable,
	void (*f)(void *), void *data,
	void *stack_base, unsigned long stack_size, void *tlsarea)
{
	struct bmk_thread *thread;
	unsigned long flags;

	thread = bmk_xmalloc(sizeof(*thread));
	bmk_memset(thread, 0, sizeof(*thread));
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

	bmk_cpu_sched_create(thread, &thread->bt_tcb, f, data,
	    stack_base, stack_size);

	thread->bt_cookie = cookie;
	thread->bt_wakeup_time = BMK_SCHED_BLOCK_INFTIME;

	inittcb(&thread->bt_tcb, tlsarea, TCBOFFSET);
	initcurrent(tlsarea, thread);

	flags = bmk_platform_splhigh();
	TAILQ_INSERT_TAIL(&threads, thread, bt_entries);
	bmk_platform_splx(flags);
	set_runnable(thread);

	return thread;
}

struct bmk_thread *
bmk_sched_create(const char *name, void *cookie, int joinable,
	void (*f)(void *), void *data,
	void *stack_base, unsigned long stack_size)
{
	void *tlsarea;

	tlsarea = bmk_sched_tls_alloc();
	return bmk_sched_create_withtls(name, cookie, joinable, f, data,
	    stack_base, stack_size, tlsarea);
}

struct join_waiter {
	struct bmk_thread *jw_thread;
	struct bmk_thread *jw_wanted;
	TAILQ_ENTRY(join_waiter) jw_entries;
};
static TAILQ_HEAD(, join_waiter) joinwq = TAILQ_HEAD_INITIALIZER(joinwq);

void
bmk_sched_exit_withtls(void)
{
	struct bmk_thread *thread = bmk_current;
	struct join_waiter *jw_iter;
	unsigned long flags;

	/* if joinable, gate until we are allowed to exit */
	flags = bmk_platform_splhigh();
	while (thread->bt_flags & THREAD_MUSTJOIN) {
		thread->bt_flags |= THREAD_JOINED;
		bmk_platform_splx(flags);

		/* see if the joiner is already there */
		TAILQ_FOREACH(jw_iter, &joinwq, jw_entries) {
			if (jw_iter->jw_wanted == thread) {
				bmk_sched_wake(jw_iter->jw_thread);
				break;
			}
		}
		bmk_sched_block(thread);
		bmk_sched();
		flags = bmk_platform_splhigh();
	}

	/* Remove from the thread list */
	TAILQ_REMOVE(&threads, thread, bt_entries);
	clear_runnable(thread);
	thread->bt_flags |= THREAD_DEAD;

	/* Put onto exited list */
	TAILQ_INSERT_HEAD(&zombies, thread, bt_entries);
	bmk_platform_splx(flags);

	/* bye */
	bmk_sched();
	bmk_platform_halt("schedule() returned for a dead thread!\n");
}

void
bmk_sched_exit(void)
{

	bmk_sched_tls_free((void *)bmk_current->bt_tcb.btcb_tp);
	bmk_sched_exit_withtls();
}

void
bmk_sched_join(struct bmk_thread *joinable)
{
	struct join_waiter jw;
	struct bmk_thread *thread = bmk_current;
	unsigned long flags;

	bmk_assert(joinable->bt_flags & THREAD_MUSTJOIN);

	flags = bmk_platform_splhigh();
	/* wait for exiting thread to hit thread_exit() */
	while ((joinable->bt_flags & THREAD_JOINED) == 0) {
		bmk_platform_splx(flags);

		jw.jw_thread = thread;
		jw.jw_wanted = joinable;
		TAILQ_INSERT_TAIL(&joinwq, &jw, jw_entries);
		bmk_sched_block(thread);
		bmk_sched();
		TAILQ_REMOVE(&joinwq, &jw, jw_entries);

		flags = bmk_platform_splhigh();
	}

	/* signal exiting thread that we have seen it and it may now exit */
	bmk_assert(joinable->bt_flags & THREAD_JOINED);
	joinable->bt_flags &= ~THREAD_MUSTJOIN;
	bmk_platform_splx(flags);

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

	bmk_sched_block_timeout(thread, BMK_SCHED_BLOCK_INFTIME);
}

int
bmk_sched_nanosleep_abstime(bmk_time_t nsec)
{
	struct bmk_thread *thread = bmk_current;
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

	return bmk_sched_nanosleep_abstime(nsec
	    + bmk_platform_clock_monotonic());
}

void
bmk_sched_wake(struct bmk_thread *thread)
{

	thread->bt_wakeup_time = BMK_SCHED_BLOCK_INFTIME;
	set_runnable(thread);
}

/*
 * Calculate offset of bmk_current early, so that we can use it
 * in thread creation.  Attempt to not depend on allocating the
 * TLS area so that we don't have to have malloc initialized.
 * We will properly initialize TLS for the main thread later
 * when we start the main thread (which is not necessarily the
 * first thread that we create).
 */
void
bmk_sched_init(void)
{
	unsigned long tlsinit;
	struct bmk_tcb tcbinit;

	inittcb(&tcbinit, &tlsinit, 0);
	bmk_platform_cpu_sched_settls(&tcbinit);

	/*
	 * Not sure if this is necessary, but better to be
	 * Marvin the Paranoid Paradroid than get eaten by 999
	 */
	__asm__ __volatile__("" ::: "memory");
	bmk_curoff = (unsigned long)&bmk_current - (unsigned long)&tlsinit;
	__asm__ __volatile__("" ::: "memory");

	/*
	 * Set TLS back to 0 so that it's easier to catch someone trying
	 * to use it until we get TLS really initialized.
	 */
	tcbinit.btcb_tp = 0;
	bmk_platform_cpu_sched_settls(&tcbinit);
}

void __attribute__((noreturn))
bmk_sched_startmain(void (*mainfun)(void *), void *arg)
{
	struct bmk_thread *mainthread;
	struct bmk_thread initthread;

	bmk_memset(&initthread, 0, sizeof(initthread));
	bmk_strcpy(initthread.bt_name, "init");

	mainthread = bmk_sched_create("main", NULL, 0,
	    mainfun, arg, NULL, 0);
	if (mainthread == NULL)
		bmk_platform_halt("failed to create main thread");

	sched_switch(&initthread, mainthread);

	bmk_platform_halt("bmk_sched_init unreachable");
}

void
bmk_sched_set_hook(void (*f)(void *, void *))
{

	scheduler_hook = f;
}

struct bmk_thread *
bmk_sched_init_mainlwp(void *cookie)
{

	bmk_current->bt_cookie = cookie;
	return bmk_current;
}

const char *
bmk_sched_threadname(struct bmk_thread *thread)
{

	return thread->bt_name;
}

int *
bmk_sched_geterrno(void)
{

	return &bmk_current->bt_errno;
}

void
bmk_sched_settls(struct bmk_thread *thread, unsigned int which, void *value)
{

	if (which >= TLS_COUNT) {
		bmk_platform_halt("out of bmk sched tls space");
	}
	thread->bt_tls[which] = value;
}

void *
bmk_sched_gettls(struct bmk_thread *thread, unsigned int which)
{

	if (which >= TLS_COUNT) {
		bmk_platform_halt("out of bmk sched tls space");
	}
	return thread->bt_tls[which];
}

void
bmk_sched_yield(void)
{
	struct bmk_thread *current = bmk_current;

	TAILQ_REMOVE(&threads, current, bt_entries);
	TAILQ_INSERT_TAIL(&threads, current, bt_entries);
	bmk_sched();
}
