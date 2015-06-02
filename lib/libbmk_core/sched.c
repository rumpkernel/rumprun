/*-
 * Copyright (c) 2015 Antti Kantee.  All Rights Reserved.
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
 * Historically based on the Xen Mini-OS scheduler by Grzegorz Milos,
 * rewritten to deal with multiple infrequently running threads in the
 * current reincarnation.
 */

#include <bmk-core/core.h>
#include <bmk-core/errno.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <bmk-core/queue.h>
#include <bmk-core/string.h>
#include <bmk-core/sched.h>

/*
 * sleep for how long if there's absolutely nothing to do
 * (default 1s)
 */
#define BLOCKTIME_MAX (1*1000*1000*1000)

#define NAME_MAXLEN 16

/* flags and their meanings + invariants */
#define THR_RUNQ	0x0001		/* on runq, can be run		*/
#define THR_TIMEQ	0x0002		/* on timeq, blocked w/ timeout	*/
#define THR_BLOCKQ	0x0004		/* on blockq, indefinite block	*/
#define THR_QMASK	0x0007
#define THR_RUNNING	0x0008		/* no queue, thread == current	*/

#define THR_TIMEDOUT	0x0010
#define THR_MUSTJOIN	0x0020
#define THR_JOINED	0x0040

#define THR_EXTSTACK	0x0100
#define THR_DEAD	0x0200
#define THR_BLOCKPREP	0x0400

extern const char _tdata_start[], _tdata_end[];
extern const char _tbss_start[], _tbss_end[];
#define TDATASIZE (_tdata_end - _tdata_start)
#define TBSSSIZE (_tbss_end - _tbss_start)
#define TCBOFFSET \
    (((TDATASIZE + TBSSSIZE + sizeof(void *)-1)/sizeof(void *))*sizeof(void *))
#define TLSAREASIZE (TCBOFFSET + BMK_TLS_EXTRA)

struct bmk_thread {
	char bt_name[NAME_MAXLEN];

	bmk_time_t bt_wakeup_time;

	int bt_flags;
	int bt_errno;

	void *bt_stackbase;

	void *bt_cookie;

	/* MD thread control block */
	struct bmk_tcb bt_tcb;

	TAILQ_ENTRY(bmk_thread) bt_schedq;
	TAILQ_ENTRY(bmk_thread) bt_threadq;
};
__thread struct bmk_thread *bmk_current;

TAILQ_HEAD(threadqueue, bmk_thread);
static struct threadqueue threadq = TAILQ_HEAD_INITIALIZER(threadq);
static struct threadqueue zombieq = TAILQ_HEAD_INITIALIZER(zombieq);

/*
 * We have 3 different queues for theoretically runnable threads:
 * 1) runnable threads waiting to be scheduled
 * 2) threads waiting for a timeout to expire (or to be woken up)
 * 3) threads waiting indefinitely for a wakeup
 *
 * Rules: while running, threads are on no schedq.  Threads can block
 *        only themselves (though that needs revisiting for "suspend").
 *        when blocked, threads will move either to blockq or timeq.
 *        When a thread is woken up (possibly by itself of a timeout
 *        expires), the thread will move to the runnable queue.  Wakeups
 *        while a thread is already in the runnable queue or while
 *        running (via interrupt handler) have no effect.
 */
static struct threadqueue runq = TAILQ_HEAD_INITIALIZER(runq);
static struct threadqueue blockq = TAILQ_HEAD_INITIALIZER(blockq);
static struct threadqueue timeq = TAILQ_HEAD_INITIALIZER(timeq);

static void (*scheduler_hook)(void *, void *);

static void
print_threadinfo(struct bmk_thread *thread)
{

	bmk_printf("thread \"%s\" at %p, flags 0x%x\n",
	    thread->bt_name, thread, thread->bt_flags);
}

static inline void
setflags(struct bmk_thread *thread, int add, int remove)
{

	thread->bt_flags &= ~remove;
	thread->bt_flags |= add;
}

static void
set_runnable(struct bmk_thread *thread)
{
	struct threadqueue *tq;
	int tflags;
	int flags;

	tflags = thread->bt_flags;
	/*
	 * Already runnable?  Nothing to do, then.
	 */
	if ((tflags & THR_RUNQ) == THR_RUNQ)
		return;

	/* get current queue */
	switch (tflags & THR_QMASK) {
	case THR_TIMEQ:
		tq = &timeq;
		break;
	case THR_BLOCKQ:
		tq = &blockq;
		break;
	default:
		/*
		 * Are we running and not blocked?  Might be that we were
		 * called from an interrupt handler.  Can just ignore
		 * this whole thing.
		 */
		if ((tflags & (THR_RUNNING|THR_QMASK)) == THR_RUNNING)
			return;

		print_threadinfo(thread);
		bmk_platform_halt("invalid thread queue");
	}

	/*
	 * Else, target was blocked and need to make it runnable
	 */
	flags = bmk_platform_splhigh();
	TAILQ_REMOVE(tq, thread, bt_schedq);
	setflags(thread, THR_RUNQ, THR_QMASK);
	TAILQ_INSERT_TAIL(&runq, thread, bt_schedq);
	bmk_platform_splx(flags);
}

/*
 * Insert thread into timeq at the correct place.
 */
static void
timeq_sorted_insert(struct bmk_thread *thread)
{
	struct bmk_thread *iter;

	bmk_assert(thread->bt_wakeup_time != BMK_SCHED_BLOCK_INFTIME);

	/* case1: no others */
	if (TAILQ_EMPTY(&timeq)) {
		TAILQ_INSERT_HEAD(&timeq, thread, bt_schedq);
		return;
	}

	/* case2: not last in queue */
	TAILQ_FOREACH(iter, &timeq, bt_schedq) {
		if (iter->bt_wakeup_time > thread->bt_wakeup_time) {
			TAILQ_INSERT_BEFORE(iter, thread, bt_schedq);
			return;
		}
	}

	/* case3: last in queue with greatest current timeout */
	bmk_assert(TAILQ_LAST(&timeq, threadqueue)->bt_wakeup_time
	    < thread->bt_wakeup_time);
	TAILQ_INSERT_TAIL(&timeq, thread, bt_schedq);
}

/*
 * Called with interrupts disabled
 */
static void
clear_runnable(void)
{
	struct bmk_thread *thread = bmk_current;
	int newfl;

	bmk_assert(thread->bt_flags & THR_RUNNING);

	/*
	 * Currently we require that a thread will block only
	 * once before calling the scheduler.
	 */
	bmk_assert((thread->bt_flags & THR_RUNQ) == 0);

	newfl = thread->bt_flags;
	if (thread->bt_wakeup_time != BMK_SCHED_BLOCK_INFTIME) {
		newfl |= THR_TIMEQ;
		timeq_sorted_insert(thread);
	} else {
		newfl |= THR_BLOCKQ;
		TAILQ_INSERT_TAIL(&blockq, thread, bt_schedq);
	}
	thread->bt_flags = newfl;
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

void
bmk_sched_dumpqueue(void)
{
	struct bmk_thread *thr;

	bmk_printf("BEGIN runq dump\n");
	TAILQ_FOREACH(thr, &runq, bt_schedq) {
		print_threadinfo(thr);
	}
	bmk_printf("END runq dump\n");

	bmk_printf("BEGIN timeq dump\n");
	TAILQ_FOREACH(thr, &timeq, bt_schedq) {
		print_threadinfo(thr);
	}
	bmk_printf("END timeq dump\n");

	bmk_printf("BEGIN blockq dump\n");
	TAILQ_FOREACH(thr, &blockq, bt_schedq) {
		print_threadinfo(thr);
	}
	bmk_printf("END blockq dump\n");
}

static void
sched_switch(struct bmk_thread *prev, struct bmk_thread *next)
{

	bmk_assert(next->bt_flags & THR_RUNNING);
	bmk_assert((next->bt_flags & THR_QMASK) == 0);

	if (scheduler_hook)
		scheduler_hook(prev->bt_cookie, next->bt_cookie);
	bmk_platform_cpu_sched_settls(&next->bt_tcb);
	bmk_cpu_sched_switch(&prev->bt_tcb, &next->bt_tcb);
}

static void
schedule(void)
{
	struct bmk_thread *prev, *next, *thread;
	unsigned long flags;

	prev = bmk_current;

	flags = bmk_platform_splhigh();
	if (flags) {
		bmk_platform_halt("schedule() called at !spl0");
	}
	for (;;) {
		bmk_time_t curtime, waketime;

		curtime = bmk_platform_clock_monotonic();
		waketime = curtime + BLOCKTIME_MAX;

		/*
		 * Process timeout queue first by moving threads onto
		 * the runqueue if their timeouts have expired.  Since
		 * the timeouts are sorted, we process until we hit the
		 * first one which will not be woked up.
		 */
		while ((thread = TAILQ_FIRST(&timeq)) != NULL) {
			if (thread->bt_wakeup_time <= curtime) {
				/*
				 * move thread to runqueue.
				 * threads will run in inverse order of timeout
				 * expiry.  not sure if that matters or not.
				 */
				thread->bt_flags |= THR_TIMEDOUT;
				bmk_sched_wake(thread);
			} else {
				if (thread->bt_wakeup_time < waketime)
					waketime = thread->bt_wakeup_time;
				break;
			}
		}

		if ((next = TAILQ_FIRST(&runq)) != NULL) {
			bmk_assert(next->bt_flags & THR_RUNQ);
			bmk_assert((next->bt_flags & THR_DEAD) == 0);
			break;
		}

		/* nothing to run.  enable interrupts and sleep. */
		bmk_platform_block(waketime);
	}
	/* now we're committed to letting "next" run next */
	setflags(prev, 0, THR_RUNNING);

	TAILQ_REMOVE(&runq, next, bt_schedq);
	setflags(next, THR_RUNNING, THR_RUNQ);
	bmk_platform_splx(flags);

	/*
	 * No switch can happen if:
	 *  + timeout expired while we were in here
	 *  + interrupt handler woke us up before anything else was scheduled
	 */
	if (prev != next) {
		sched_switch(prev, next);
	}

	/*
	 * Reaper.  This always runs in the context of the first "non-virgin"
	 * thread that was scheduled after the current thread decided to exit.
	 */
	while ((thread = TAILQ_FIRST(&zombieq)) != NULL) {
		TAILQ_REMOVE(&zombieq, thread, bt_threadq);
		if ((thread->bt_flags & THR_EXTSTACK) == 0)
			stackfree(thread);
		bmk_memfree(thread, BMK_MEMWHO_WIREDBMK);
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

	tlsmem = bmk_memalloc(TLSAREASIZE, 0, BMK_MEMWHO_WIREDBMK);
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
	bmk_memfree(mem, BMK_MEMWHO_WIREDBMK);
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

	thread = bmk_xmalloc_bmk(sizeof(*thread));
	bmk_memset(thread, 0, sizeof(*thread));
	bmk_strncpy(thread->bt_name, name, sizeof(thread->bt_name)-1);

	if (!stack_base) {
		bmk_assert(stack_size == 0);
		stackalloc(&stack_base, &stack_size);
	} else {
		thread->bt_flags = THR_EXTSTACK;
	}
	thread->bt_stackbase = stack_base;
	if (joinable)
		thread->bt_flags |= THR_MUSTJOIN;

	bmk_cpu_sched_create(thread, &thread->bt_tcb, f, data,
	    stack_base, stack_size);

	thread->bt_cookie = cookie;
	thread->bt_wakeup_time = BMK_SCHED_BLOCK_INFTIME;

	inittcb(&thread->bt_tcb, tlsarea, TCBOFFSET);
	initcurrent(tlsarea, thread);

	TAILQ_INSERT_TAIL(&threadq, thread, bt_threadq);

	/* set runnable manually, we don't satisfy invariants yet */
	flags = bmk_platform_splhigh();
	TAILQ_INSERT_TAIL(&runq, thread, bt_schedq);
	thread->bt_flags |= THR_RUNQ;
	bmk_platform_splx(flags);

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
	while (thread->bt_flags & THR_MUSTJOIN) {
		thread->bt_flags |= THR_JOINED;
		bmk_platform_splx(flags);

		/* see if the joiner is already there */
		TAILQ_FOREACH(jw_iter, &joinwq, jw_entries) {
			if (jw_iter->jw_wanted == thread) {
				bmk_sched_wake(jw_iter->jw_thread);
				break;
			}
		}
		bmk_sched_blockprepare();
		bmk_sched_block();
		flags = bmk_platform_splhigh();
	}

	/* Remove from the thread list */
	bmk_assert((thread->bt_flags & THR_QMASK) == 0);
	TAILQ_REMOVE(&threadq, thread, bt_threadq);
	setflags(thread, THR_DEAD, THR_RUNNING);

	/* Put onto exited list */
	TAILQ_INSERT_HEAD(&zombieq, thread, bt_threadq);
	bmk_platform_splx(flags);

	/* bye */
	schedule();
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

	bmk_assert(joinable->bt_flags & THR_MUSTJOIN);

	flags = bmk_platform_splhigh();
	/* wait for exiting thread to hit thread_exit() */
	while ((joinable->bt_flags & THR_JOINED) == 0) {
		bmk_platform_splx(flags);

		jw.jw_thread = thread;
		jw.jw_wanted = joinable;
		TAILQ_INSERT_TAIL(&joinwq, &jw, jw_entries);
		bmk_sched_blockprepare();
		bmk_sched_block();
		TAILQ_REMOVE(&joinwq, &jw, jw_entries);

		flags = bmk_platform_splhigh();
	}

	/* signal exiting thread that we have seen it and it may now exit */
	bmk_assert(joinable->bt_flags & THR_JOINED);
	joinable->bt_flags &= ~THR_MUSTJOIN;
	bmk_platform_splx(flags);

	bmk_sched_wake(joinable);
}

/*
 * These suspend calls are different from block calls in the that
 * can be used to block other threads.  The only reason we need these
 * was because someone was clever enough to invent _np interfaces for
 * libpthread which allow randomly suspending other threads.
 */
void
bmk_sched_suspend(struct bmk_thread *thread)
{

	bmk_platform_halt("sched_suspend unimplemented");
}

void
bmk_sched_unsuspend(struct bmk_thread *thread)
{

	bmk_platform_halt("sched_unsuspend unimplemented");
}

void
bmk_sched_blockprepare_timeout(bmk_time_t deadline)
{
	struct bmk_thread *thread = bmk_current;
	int flags;

	bmk_assert((thread->bt_flags & THR_BLOCKPREP) == 0);

	flags = bmk_platform_splhigh();
	thread->bt_wakeup_time = deadline;
	thread->bt_flags |= THR_BLOCKPREP;
	clear_runnable();
	bmk_platform_splx(flags);
}

void
bmk_sched_blockprepare(void)
{

	bmk_sched_blockprepare_timeout(BMK_SCHED_BLOCK_INFTIME);
}

int
bmk_sched_block(void)
{
	struct bmk_thread *thread = bmk_current;
	int tflags;

	bmk_assert((thread->bt_flags & THR_TIMEDOUT) == 0);
	bmk_assert(thread->bt_flags & THR_BLOCKPREP);

	schedule();

	tflags = thread->bt_flags;
	thread->bt_flags &= ~(THR_TIMEDOUT | THR_BLOCKPREP);

	return tflags & THR_TIMEDOUT ? BMK_ETIMEDOUT : 0;
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
	 * Not sure if the membars are necessary, but better to be
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

	/*
	 * Manually switch to mainthread without going through
	 * bmk_sched (avoids confusion with bmk_current).
	 */
	TAILQ_REMOVE(&runq, mainthread, bt_schedq);
	setflags(mainthread, THR_RUNNING, THR_RUNQ);
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

/*
 * XXX: this does not really belong here, but libbmk_rumpuser needs
 * to be able to set an errno, so we can't push it into libc without
 * violating abstraction layers.
 */
int *
bmk_sched_geterrno(void)
{

	return &bmk_current->bt_errno;
}

void
bmk_sched_yield(void)
{
	struct bmk_thread *thread = bmk_current;
	int flags;

	bmk_assert(thread->bt_flags & THR_RUNNING);

	/* make schedulable and re-insert into runqueue */
	flags = bmk_platform_splhigh();
	setflags(thread, THR_RUNQ, THR_RUNNING);
	TAILQ_INSERT_TAIL(&runq, thread, bt_schedq);
	bmk_platform_splx(flags);

	schedule();
}
