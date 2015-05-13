/*-
 * Copyright (c) 2014, 2015 Antti Kantee.  All Rights Reserved.
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

#define _lwp_park ___lwp_park60
#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/lwpctl.h>
#include <sys/lwp.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/tls.h>

#include <assert.h>
#include <errno.h>
#include <lwp.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bmk-core/core.h>
#include <bmk-core/sched.h>

#include <rumprun-base/makelwp.h>

#include "rumprun-private.h"

struct rumprun_lwp {
	struct bmk_thread *rl_thread;
	int rl_lwpid;
	char rl_name[MAXCOMLEN+1];

	struct lwpctl rl_lwpctl;
	int rl_no_parking_hare;	/* a looney tunes reference ... finally! */

	TAILQ_ENTRY(rumprun_lwp) rl_entries;
};
static TAILQ_HEAD(, rumprun_lwp) all_lwp = TAILQ_HEAD_INITIALIZER(all_lwp);
static __thread struct rumprun_lwp *me;

#define FIRST_LWPID 1
static int curlwpid = FIRST_LWPID;

static struct rumprun_lwp mainthread = {
	.rl_lwpid = FIRST_LWPID,
};

static ptrdiff_t meoff;
static void
assignme(void *tcb, struct rumprun_lwp *value)
{
	struct rumprun_lwp **dst = (void *)((uintptr_t)tcb + meoff);

	*dst = value;
}

int
_lwp_ctl(int ctl, struct lwpctl **data)
{

	*data = (struct lwpctl *)&me->rl_lwpctl;
	return 0;
}

int
rumprun_makelwp(void (*start)(void *), void *arg, void *private,
	void *stack_base, size_t stack_size, unsigned long flag, lwpid_t *lid)
{
	struct rumprun_lwp *rl;

	rl = calloc(1, sizeof(*rl));
	if (rl == NULL)
		return errno;
	assignme(private, rl);

	rl->rl_lwpid = ++curlwpid;
	rl->rl_thread = bmk_sched_create_withtls("lwp", rl, 0,
	    start, arg, stack_base, stack_size, private);
	if (rl->rl_thread == NULL) {
		free(rl);
		return EBUSY; /* ??? */
	}

	*lid = rl->rl_lwpid;
	TAILQ_INSERT_TAIL(&all_lwp, rl, rl_entries);

	return 0;
}

static struct rumprun_lwp *
lwpid2rl(lwpid_t lid)
{
	struct rumprun_lwp *rl;

	if (lid == 0)
		return &mainthread;
	TAILQ_FOREACH(rl, &all_lwp, rl_entries) {
		if (rl->rl_lwpid == lid)
			return rl;
	}
	return NULL;
}

int
_lwp_unpark(lwpid_t lid, const void *hint)
{
	struct rumprun_lwp *rl;

	if ((rl = lwpid2rl(lid)) == NULL) {
		return -1;
	}

	bmk_sched_wake(rl->rl_thread);
	return 0;
}

ssize_t
_lwp_unpark_all(const lwpid_t *targets, size_t ntargets, const void *hint)
{
	ssize_t rv;

	if (targets == NULL)
		return 1024;

	rv = ntargets;
	while (ntargets--) {
		if (_lwp_unpark(*targets, NULL) != 0)
			rv--;
		targets++;
	}
	//assert(rv >= 0);
	return rv;
}

/*
 * called by the scheduler when a context switch is made
 * nb. cookie is null when non-lwp threads are being run
 */
static void
schedhook(void *prevcookie, void *nextcookie)
{
	struct rumprun_lwp *prev, *next;

	prev = prevcookie;
	next = nextcookie;

	if (prev && prev->rl_lwpctl.lc_curcpu != LWPCTL_CPU_EXITED) {
		prev->rl_lwpctl.lc_curcpu = LWPCTL_CPU_NONE;
	}
	if (next) {
		next->rl_lwpctl.lc_curcpu = 0;
		next->rl_lwpctl.lc_pctr++;
	}
}

void
rumprun_lwp_init(void)
{
	void *tcb = bmk_sched_gettcb();

	bmk_sched_set_hook(schedhook);

	meoff = (uintptr_t)&me - (uintptr_t)tcb;
	assignme(tcb, &mainthread);
	mainthread.rl_thread = bmk_sched_init_mainlwp(&mainthread);

	TAILQ_INSERT_TAIL(&all_lwp, me, rl_entries);
}

int
_lwp_park(clockid_t clock_id, int flags, const struct timespec *ts,
	lwpid_t unpark, const void *hint, const void *unparkhint)
{
	int rv;

	if (unpark)
		_lwp_unpark(unpark, unparkhint);

	if (me->rl_no_parking_hare) {
		me->rl_no_parking_hare = 0;
		return 0;
	}

	if (ts) {
		bmk_time_t nsecs = ts->tv_sec*1000*1000*1000 + ts->tv_nsec;

		if (flags & TIMER_ABSTIME) {
			nsecs -= bmk_platform_clock_epochoffset();
			rv = bmk_sched_nanosleep_abstime(nsecs);
		} else {
			rv = bmk_sched_nanosleep(nsecs);
		}
		if (rv) {
			rv = ETIMEDOUT;
		}
	} else {
		bmk_sched_blockprepare();
		bmk_sched();
		rv = 0;
	}

	if (rv) {
		errno = rv;
		rv = -1;
	}
	return rv;
}

int
_lwp_exit(void)
{

	me->rl_lwpctl.lc_curcpu = LWPCTL_CPU_EXITED;
	TAILQ_REMOVE(&all_lwp, me, rl_entries);

	/* could just assign it here, but for symmetry! */
	assignme(bmk_sched_gettcb(), NULL);

	bmk_sched_exit_withtls();

	return 0;
}

int
_lwp_continue(lwpid_t lid)
{
	struct rumprun_lwp *rl;

	if ((rl = lwpid2rl(lid)) == NULL) {
		return ESRCH;
	}

	bmk_sched_unsuspend(rl->rl_thread);
	return 0;
}

int
_lwp_suspend(lwpid_t lid)
{
	struct rumprun_lwp *rl;

	if ((rl = lwpid2rl(lid)) == NULL) {
		return ESRCH;
	}

	bmk_sched_suspend(rl->rl_thread);
	return 0;
}

int
_lwp_wakeup(lwpid_t lid)
{
	struct rumprun_lwp *rl;

	if ((rl = lwpid2rl(lid)) == NULL)
		return ESRCH;

	bmk_sched_wake(rl->rl_thread);
	return 0;
}

int
_lwp_setname(lwpid_t lid, const char *name)
{
	struct rumprun_lwp *rl;

	if ((rl = lwpid2rl(lid)) == NULL)
		return ESRCH;
	strlcpy(rl->rl_name, name, sizeof(rl->rl_name));

	return 0;
}

lwpid_t
_lwp_self(void)
{

	return me->rl_lwpid;
}

/* XXX: messy.  see sched.h, libc, libpthread, and all over */
int _sys_sched_yield(void);
int
_sys_sched_yield(void)
{

	bmk_sched_yield();
	return 0;
}
__weak_alias(sched_yield,_sys_sched_yield);

struct tls_tcb *
_rtld_tls_allocate(void)
{

	return bmk_sched_tls_alloc();
}

void
_rtld_tls_free(struct tls_tcb *arg)
{

	return bmk_sched_tls_free(arg);
}

void *
_lwp_getprivate(void)
{

	return bmk_sched_gettcb();
}

void _lwpnullop(void);
void _lwpnullop(void) { }

void _lwpabort(void);
void __dead
_lwpabort(void)
{

	printf("_lwpabort() called\n");
	_exit(1);
}

__strong_alias(_sys_setcontext,_lwpabort);
__strong_alias(_lwp_kill,_lwpabort);

__strong_alias(__libc_static_tls_setup,_lwpnullop);

int rasctl(void);
int rasctl(void) { return ENOSYS; }

/*
 * There is ongoing work to support these in the rump kernel,
 * so I will just stub them out for now.
 */
__strong_alias(_sched_getaffinity,_lwpnullop);
__strong_alias(_sched_getparam,_lwpnullop);
__strong_alias(_sched_setaffinity,_lwpnullop);
__strong_alias(_sched_setparam,_lwpnullop);
