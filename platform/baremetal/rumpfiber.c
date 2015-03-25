/*
 * Copyright (c) 2007-2013 Antti Kantee.  All Rights Reserved.
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

#include <bmk/types.h>
#include <bmk/kernel.h>
#include <bmk/memalloc.h>
#include <bmk/queue.h>
#include <bmk/sched.h>

#include <bmk-core/string.h>

#define LIBRUMPUSER
#include <rump/rumpuser.h>

#include "rumpuser_int.h"

#define BMK_TLS_RUMPLWP		0
#define BMK_TLS_USERLWP		1

struct rumpuser_hyperup rumpuser__hyp;
int
rumpuser_init(int version, const struct rumpuser_hyperup *hyp)
{

	if (version != RUMPUSER_VERSION) {
		return BMK_EINVAL;
	}

        rumpuser__hyp = *hyp;

        return 0;
}

void
rumpuser_seterrno(int error)
{
	int *threrr = bmk_sched_geterrno();

	*threrr = error;
}

/* thread functions */

TAILQ_HEAD(waithead, waiter);
struct waiter {
	struct bmk_thread *who;
	TAILQ_ENTRY(waiter) entries;
	int onlist;
};

static int
wait(struct waithead *wh, uint64_t nsec)
{
	struct waiter w;

	w.who = bmk_sched_current();
	TAILQ_INSERT_TAIL(wh, &w, entries);
	w.onlist = 1;
	bmk_sched_block(w.who);
	if (nsec)
		bmk_sched_setwakeup(w.who, bmk_cpu_clock_now() + nsec);
	bmk_sched();

	/* woken up by timeout? */
	if (w.onlist)
		TAILQ_REMOVE(wh, &w, entries);

	return w.onlist ? BMK_ETIMEDOUT : 0;
}

static void
wakeup_one(struct waithead *wh)
{
	struct waiter *w;

	if ((w = TAILQ_FIRST(wh)) != NULL) {
		TAILQ_REMOVE(wh, w, entries);
		w->onlist = 0;
		bmk_sched_wake(w->who);
	}
}

static void
wakeup_all(struct waithead *wh)
{
	struct waiter *w;

	while ((w = TAILQ_FIRST(wh)) != NULL) {
		TAILQ_REMOVE(wh, w, entries);
		w->onlist = 0;
		bmk_sched_wake(w->who);
	}
}

int
rumpuser_thread_create(void *(*f)(void *), void *arg, const char *thrname,
        int joinable, int pri, int cpuidx, void **tptr)
{
        struct bmk_thread *thr;

        thr = bmk_sched_create(thrname, NULL, joinable,
	    (void (*)(void *))f, arg, NULL, 0);
        if (!thr)
                return BMK_EINVAL;

        *tptr = thr;
        return 0;
}

void
rumpuser_thread_exit(void)
{

        bmk_sched_exit();
}

int
rumpuser_thread_join(void *p)
{

        bmk_sched_join(p);
        return 0;
}

struct rumpuser_mtx {
	struct waithead waiters;
	int v;
	int flags;
	struct lwp *o;
};

void
rumpuser_mutex_init(struct rumpuser_mtx **mtxp, int flags)
{
	struct rumpuser_mtx *mtx;

	mtx = bmk_xmalloc(sizeof(*mtx));
	bmk_memset(mtx, 0, sizeof(*mtx));
	mtx->flags = flags;
	TAILQ_INIT(&mtx->waiters);
	*mtxp = mtx;
}

void
rumpuser_mutex_enter(struct rumpuser_mtx *mtx)
{
	int nlocks;

	if (rumpuser_mutex_tryenter(mtx) != 0) {
		rumpkern_unsched(&nlocks, NULL);
		while (rumpuser_mutex_tryenter(mtx) != 0)
			wait(&mtx->waiters, 0);
		rumpkern_sched(nlocks, NULL);
	}
}

void
rumpuser_mutex_enter_nowrap(struct rumpuser_mtx *mtx)
{
	int rv;

	rv = rumpuser_mutex_tryenter(mtx);
	/* one VCPU supported, no preemption => must succeed */
	if (rv != 0) {
		panic("spin mutex error");
	}
}

int
rumpuser_mutex_tryenter(struct rumpuser_mtx *mtx)
{
	struct lwp *l = bmk_sched_gettls(bmk_sched_current(), BMK_TLS_RUMPLWP);

	if (mtx->v && mtx->o != l)
		return BMK_EBUSY;

	mtx->v++;
	mtx->o = l;

	return 0;
}

void
rumpuser_mutex_exit(struct rumpuser_mtx *mtx)
{

	assert(mtx->v > 0);
	if (--mtx->v == 0) {
		mtx->o = NULL;
		wakeup_one(&mtx->waiters);
	}
}

void
rumpuser_mutex_destroy(struct rumpuser_mtx *mtx)
{

	assert(TAILQ_EMPTY(&mtx->waiters) && mtx->o == NULL);
	bmk_memfree(mtx);
}

void
rumpuser_mutex_owner(struct rumpuser_mtx *mtx, struct lwp **lp)
{

	*lp = mtx->o;
}

struct rumpuser_rw {
	struct waithead rwait;
	struct waithead wwait;
	int v;
	struct lwp *o;
};

void
rumpuser_rw_init(struct rumpuser_rw **rwp)
{
	struct rumpuser_rw *rw;

	rw = bmk_xmalloc(sizeof(*rw));
	bmk_memset(rw, 0, sizeof(*rw));
	TAILQ_INIT(&rw->rwait);
	TAILQ_INIT(&rw->wwait);

	*rwp = rw;
}

void
rumpuser_rw_enter(int enum_rumprwlock, struct rumpuser_rw *rw)
{
	enum rumprwlock lk = enum_rumprwlock;
	struct waithead *w = NULL;
	int nlocks;

	switch (lk) {
	case RUMPUSER_RW_WRITER:
		w = &rw->wwait;
		break;
	case RUMPUSER_RW_READER:
		w = &rw->rwait;
		break;
	}

	if (rumpuser_rw_tryenter(enum_rumprwlock, rw) != 0) {
		rumpkern_unsched(&nlocks, NULL);
		while (rumpuser_rw_tryenter(enum_rumprwlock, rw) != 0)
			wait(w, 0);
		rumpkern_sched(nlocks, NULL);
	}
}

int
rumpuser_rw_tryenter(int enum_rumprwlock, struct rumpuser_rw *rw)
{
	enum rumprwlock lk = enum_rumprwlock;
	int rv;

	switch (lk) {
	case RUMPUSER_RW_WRITER:
		if (rw->o == NULL) {
			rw->o = rumpuser_curlwp();
			rv = 0;
		} else {
			rv = BMK_EBUSY;
		}
		break;
	case RUMPUSER_RW_READER:
		if (rw->o == NULL && TAILQ_EMPTY(&rw->wwait)) {
			rw->v++;
			rv = 0;
		} else {
			rv = BMK_EBUSY;
		}
		break;
	default:
		rv = BMK_EINVAL;
	}

	return rv;
}

void
rumpuser_rw_exit(struct rumpuser_rw *rw)
{

	if (rw->o) {
		rw->o = NULL;
	} else {
		rw->v--;
	}

	/* standard procedure, don't let readers starve out writers */
	if (!TAILQ_EMPTY(&rw->wwait)) {
		if (rw->o == NULL)
			wakeup_one(&rw->wwait);
	} else if (!TAILQ_EMPTY(&rw->rwait) && rw->o == NULL) {
		wakeup_all(&rw->rwait);
	}
}

void
rumpuser_rw_destroy(struct rumpuser_rw *rw)
{

	bmk_memfree(rw);
}

void
rumpuser_rw_held(int enum_rumprwlock, struct rumpuser_rw *rw, int *rvp)
{
	enum rumprwlock lk = enum_rumprwlock;

	switch (lk) {
	case RUMPUSER_RW_WRITER:
		*rvp = rw->o == rumpuser_curlwp();
		break;
	case RUMPUSER_RW_READER:
		*rvp = rw->v > 0;
		break;
	}
}

void
rumpuser_rw_downgrade(struct rumpuser_rw *rw)
{

	assert(rw->o == rumpuser_curlwp());
	rw->v = -1;
}

int
rumpuser_rw_tryupgrade(struct rumpuser_rw *rw)
{

	if (rw->v == -1) {
		rw->v = 1;
		rw->o = rumpuser_curlwp();
		return 0;
	}

	return BMK_EBUSY;
}

struct rumpuser_cv {
	struct waithead waiters;
	int nwaiters;
};

void
rumpuser_cv_init(struct rumpuser_cv **cvp)
{
	struct rumpuser_cv *cv;

	cv = bmk_xmalloc(sizeof(*cv));
	bmk_memset(cv, 0, sizeof(*cv));
	TAILQ_INIT(&cv->waiters);
	*cvp = cv;
}

void
rumpuser_cv_destroy(struct rumpuser_cv *cv)
{

	assert(cv->nwaiters == 0);
	bmk_memfree(cv);
}

static void
cv_unsched(struct rumpuser_mtx *mtx, int *nlocks)
{

	rumpkern_unsched(nlocks, mtx);
	rumpuser_mutex_exit(mtx);
}

static void
cv_resched(struct rumpuser_mtx *mtx, int nlocks)
{

	/* see rumpuser(3) */
	if ((mtx->flags & (RUMPUSER_MTX_KMUTEX | RUMPUSER_MTX_SPIN)) ==
	    (RUMPUSER_MTX_KMUTEX | RUMPUSER_MTX_SPIN)) {
		rumpkern_sched(nlocks, mtx);
		rumpuser_mutex_enter_nowrap(mtx);
	} else {
		rumpuser_mutex_enter_nowrap(mtx);
		rumpkern_sched(nlocks, mtx);
	}
}

void
rumpuser_cv_wait(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx)
{
	int nlocks;

	cv->nwaiters++;
	cv_unsched(mtx, &nlocks);
	wait(&cv->waiters, 0);
	cv_resched(mtx, nlocks);
	cv->nwaiters--;
}

void
rumpuser_cv_wait_nowrap(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx)
{

	cv->nwaiters++;
	rumpuser_mutex_exit(mtx);
	wait(&cv->waiters, 0);
	rumpuser_mutex_enter_nowrap(mtx);
	cv->nwaiters--;
}

int
rumpuser_cv_timedwait(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx,
	int64_t sec, int64_t nsec)
{
	int nlocks;
	int rv;

	cv->nwaiters++;
	cv_unsched(mtx, &nlocks);
	rv = wait(&cv->waiters, sec * 1000*1000*1000ULL + nsec);
	cv_resched(mtx, nlocks);
	cv->nwaiters--;

	return rv;
}

void
rumpuser_cv_signal(struct rumpuser_cv *cv)
{

	wakeup_one(&cv->waiters);
}

void
rumpuser_cv_broadcast(struct rumpuser_cv *cv)
{

	wakeup_all(&cv->waiters);
}

void
rumpuser_cv_has_waiters(struct rumpuser_cv *cv, int *rvp)
{

	*rvp = cv->nwaiters != 0;
}

/*
 * curlwp
 */

void
rumpuser_curlwpop(int enum_rumplwpop, struct lwp *l)
{
	struct bmk_thread *thread;
	enum rumplwpop op = enum_rumplwpop;

	switch (op) {
	case RUMPUSER_LWP_CREATE:
	case RUMPUSER_LWP_DESTROY:
		break;
	case RUMPUSER_LWP_SET:
		assert(rumpuser_curlwp() == NULL);
		thread = bmk_sched_current();
		bmk_sched_settls(thread, BMK_TLS_RUMPLWP, l);
		break;
	case RUMPUSER_LWP_CLEAR:
		assert(rumpuser_curlwp() == l);
		thread = bmk_sched_current();
		bmk_sched_settls(thread, BMK_TLS_RUMPLWP, NULL);
		break;
	}
}

struct lwp *
rumpuser_curlwp(void)
{

	return bmk_sched_gettls(bmk_sched_current(), BMK_TLS_RUMPLWP);
}
