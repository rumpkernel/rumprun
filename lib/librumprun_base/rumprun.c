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

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/queue.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include <fs/tmpfs/tmpfs_args.h>

#include <bmk-core/platform.h>

#include <rumprun-base/rumprun.h>
#include <rumprun-base/config.h>

#include "rumprun-private.h"

static pthread_mutex_t w_mtx;
static pthread_cond_t w_cv;

void
rumprun_boot(char *cmdline)
{
	struct tmpfs_args ta = {
		.ta_version = TMPFS_ARGS_VERSION,
		.ta_size_max = 1*1024*1024,
		.ta_root_mode = 01777,
	};
	int tmpfserrno;

	rump_boot_setsigmodel(RUMP_SIGMODEL_IGNORE);
	rump_init();

	/* mount /tmp before we let any userspace bits run */
	rump_sys_mount(MOUNT_TMPFS, "/tmp", 0, &ta, sizeof(ta));
	tmpfserrno = errno;

	/*
	 * XXX: _netbsd_userlevel_init() should technically be called
	 * in mainbouncer() per process.  However, there's currently no way
	 * to run it per process, and besides we need a fully functional
	 * libc to run sysproxy and rumprun_config(), so we just call it
	 * here for the time being.
	 *
	 * Eventually, we of course want bootstrap process which is
	 * rumprun() internally.
	 */
	rumprun_lwp_init();
	_netbsd_userlevel_init();

	/* print tmpfs result only after we bootstrapped userspace */
	if (tmpfserrno == 0) {
		fprintf(stderr, "mounted tmpfs on /tmp\n");
	} else {
		warnx("FAILED: mount tmpfs on /tmp: %s", strerror(tmpfserrno));
	}

#ifdef RUMP_SYSPROXY
	rump_init_server("tcp://0:12345");
#endif
	_rumprun_config(cmdline);

	/*
	 * give all threads a chance to run, and ensure that the main
	 * thread has gone through a context switch
	 */
	sched_yield();

	pthread_mutex_init(&w_mtx, NULL);
	pthread_cond_init(&w_cv, NULL);
}

/*
 * XXX: we have to use pthreads as the main threads for rumprunners
 * because otherwise libpthread goes haywire because it doesn't understand
 * the concept of multiple main threads (which is sort of understandable ...)
 */
struct rumprunner {
	int (*rr_mainfun)(int, char *[]);
	int rr_argc;
	char **rr_argv;

	pthread_t rr_mainthread;

	int rr_done;

	LIST_ENTRY(rumprunner) rr_entries;
};
static LIST_HEAD(,rumprunner) rumprunners = LIST_HEAD_INITIALIZER(&rumprunners);
static int rumprun_done;

/* XXX: does not yet nuke any pthread that mainfun creates */
static void
releaseme(void *arg)
{
	struct rumprunner *rr = arg;

	rr->rr_done = 1;

	pthread_mutex_lock(&w_mtx);
	rumprun_done++;
	pthread_cond_broadcast(&w_cv);
	pthread_mutex_unlock(&w_mtx);
}

static void *
mainbouncer(void *arg)
{
	struct rumprunner *rr = arg;
	const char *progname = rr->rr_argv[0];
	int rv;

	rump_pub_lwproc_rfork(RUMP_RFFDG);

	pthread_cleanup_push(releaseme, rr);

	fprintf(stderr,"\n=== calling \"%s\" main() ===\n\n", progname);
	rv = rr->rr_mainfun(rr->rr_argc, rr->rr_argv);
	fflush(stdout);
	fprintf(stderr,"\n=== main() of \"%s\" returned %d ===\n",
	    progname, rv);

	pthread_cleanup_pop(1);

	/*
	 * XXX: missing _netbsd_userlevel_fini().  See comment in
	 * rumprun_boot()
	 */

	/* exit() calls rumprun_pub_lwproc_releaselwp() (via pthread_exit()) */
	exit(rv);
}

void *
rumprun(int (*mainfun)(int, char *[]), int argc, char *argv[])
{
	struct rumprunner *rr;
	static int called;

	if (called)
		bmk_platform_halt(">1 rumprun() calls not implemented yet");
	called = 1;

	rr = malloc(sizeof(*rr));

	/* XXX: should we deep copy argc? */
	rr->rr_mainfun = mainfun;
	rr->rr_argc = argc;
	rr->rr_argv = argv;
	rr->rr_done = 0;

	if (pthread_create(&rr->rr_mainthread, NULL, mainbouncer, rr) != 0) {
		fprintf(stderr, "rumprun: running %s failed\n", argv[0]);
		free(rr);
		return NULL;
	}
	LIST_INSERT_HEAD(&rumprunners, rr, rr_entries);

	return rr;
}

int
rumprun_wait(void *cookie)
{
	struct rumprunner *rr = cookie;
	void *retval;

	pthread_join(rr->rr_mainthread, &retval);
	assert(rumprun_done > 0);
	rumprun_done--;

	return (int)(intptr_t)retval;
}

void *
rumprun_get_finished(void)
{
	struct rumprunner *rr;

	if (LIST_EMPTY(&rumprunners))
		return NULL;

	pthread_mutex_lock(&w_mtx);
	while (rumprun_done == 0) {
		pthread_cond_wait(&w_cv, &w_mtx);
	}
	LIST_FOREACH(rr, &rumprunners, rr_entries) {
		if (rr->rr_done)
			break;
	}
	pthread_mutex_unlock(&w_mtx);
	assert(rr);

	return rr;
}

void __dead
rumprun_reboot(void)
{

	_rumprun_deconfig();
	_netbsd_userlevel_fini();
	rump_sys_reboot(0, 0);

	bmk_platform_halt("reboot returned");
}
