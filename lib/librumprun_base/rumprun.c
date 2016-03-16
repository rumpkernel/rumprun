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

#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/sysctl.h>

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

#include <bmk-core/platform.h>

#include <rumprun-base/rumprun.h>
#include <rumprun-base/config.h>

#include "rumprun-private.h"

static pthread_mutex_t w_mtx;
static pthread_cond_t w_cv;

int rumprun_enosys(void);
int
rumprun_enosys(void)
{

	return ENOSYS;
}

__weak_alias(rump_init_server,rumprun_enosys);

int rumprun_cold = 1;

void
rumprun_boot(char *cmdline)
{
	char *sysproxy;
	int rv, x;

	rump_boot_setsigmodel(RUMP_SIGMODEL_IGNORE);
	rump_init();

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

	/*
	 * We set duplicate address detection off for
	 * immediately operational DHCP addresses.
	 * (note: we don't check for errors since net.inet.ip.dad_count
	 * is not present if the networking stack isn't present)
	 */
	x = 0;
	sysctlbyname("net.inet.ip.dad_count", NULL, NULL, &x, sizeof(x));

	rumprun_config(cmdline);

	sysproxy = getenv("RUMPRUN_SYSPROXY");
	if (sysproxy) {
		if ((rv = rump_init_server(sysproxy)) != 0)
			err(1, "failed to init sysproxy at %s", sysproxy);
		printf("sysproxy listening at: %s\n", sysproxy);
	}

	/*
	 * give all threads a chance to run, and ensure that the main
	 * thread has gone through a context switch
	 */
	sched_yield();

	pthread_mutex_init(&w_mtx, NULL);
	pthread_cond_init(&w_cv, NULL);

	rumprun_cold = 0;
}

/*
 * XXX: we have to use pthreads as the main threads for rumprunners
 * because otherwise libpthread goes haywire because it doesn't understand
 * the concept of multiple main threads (which is sort of understandable ...)
 */
#define RUMPRUNNER_DONE		0x10
#define RUMPRUNNER_DAEMON	0x20
struct rumprunner {
	int (*rr_mainfun)(int, char *[]);
	int rr_argc;
	char **rr_argv;

	pthread_t rr_mainthread;
	struct lwp *rr_lwp;

	int rr_flags;

	LIST_ENTRY(rumprunner) rr_entries;
};
static LIST_HEAD(,rumprunner) rumprunners = LIST_HEAD_INITIALIZER(&rumprunners);
static int rumprun_done;

/* XXX: does not yet nuke any pthread that mainfun creates */
static void
releaseme(void *arg)
{
	struct rumprunner *rr = arg;

	pthread_mutex_lock(&w_mtx);
	rumprun_done++;
	rr->rr_flags |= RUMPRUNNER_DONE;
	pthread_cond_broadcast(&w_cv);
	pthread_mutex_unlock(&w_mtx);
}

static void *
mainbouncer(void *arg)
{
	struct rumprunner *rr = arg;
	const char *progname = rr->rr_argv[0];
	int rv;

	rump_pub_lwproc_switch(rr->rr_lwp);

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

static void
setupproc(struct rumprunner *rr, char *rr_workdir, struct rr_sysctl *sc,
	size_t nsc)
{
	static int pipein = -1;
	int pipefd[2], newpipein;
	const char *progname = rr->rr_argv[0];

	if (rump_pub_lwproc_curlwp() != NULL) {
		errx(1, "setupproc() needs support for non-implicit callers");
	}

	/* is the target output a pipe? */
	if (rr->rr_flags & RUMPRUN_EXEC_PIPE) {
		if (pipe(pipefd) == -1) {
			err(1, "cannot create pipe for %s", progname);
		}
		newpipein = pipefd[0];
	} else {
		newpipein = -1;
	}

	rump_pub_lwproc_rfork(RUMP_RFFDG);
	rr->rr_lwp = rump_pub_lwproc_curlwp();

	if (rr_workdir != NULL) {
		if (chdir(rr_workdir) == -1)
			err(1, "chdir");
	}

	/* apply per-process sysctl() values */
	if (sc != NULL) {
		for (size_t i = 0; i < nsc; i++) {
			int rc = rumprun_sysctlw(sc[i].key, sc[i].value);
			if (rc != 0) {
				errx(1, "error writing sysctl key \"%s\": %s",
						sc[i].key, strerror(rc));
			}
			free(sc[i].key);
			free(sc[i].value);
		}
		free(sc);
	}
	/* set output pipe to stdout if piping */
	if ((rr->rr_flags & RUMPRUN_EXEC_PIPE) && pipefd[1] != STDOUT_FILENO) {
		if (dup2(pipefd[1], STDOUT_FILENO) == -1)
			err(1, "dup2 stdout");
		close(pipefd[1]);
	}
	if (pipein != -1 && pipein != STDIN_FILENO) {
		if (dup2(pipein, STDIN_FILENO) == -1)
			err(1, "dup2 input");
		close(pipein);
	}

	rump_pub_lwproc_switch(NULL);

	/* pipe descriptors have been copied.  close them in parent */
	if (rr->rr_flags & RUMPRUN_EXEC_PIPE) {
		close(pipefd[1]);
	}
	if (pipein != -1) {
		close(pipein);
	}

	pipein = newpipein;
}

void *
rumprun(int flags, int (*mainfun)(int, char *[]), int argc, char *argv[],
	char *workdir, struct rr_sysctl *sc, size_t nsc)
{
	struct rumprunner *rr;

	rr = malloc(sizeof(*rr));

	/* XXX: should we deep copy argc? */
	rr->rr_mainfun = mainfun;
	rr->rr_argc = argc;
	rr->rr_argv = argv;
	rr->rr_flags = flags; /* XXX */

	setupproc(rr, workdir, sc, nsc);

	if (pthread_create(&rr->rr_mainthread, NULL, mainbouncer, rr) != 0) {
		fprintf(stderr, "rumprun: running %s failed\n", argv[0]);
		free(rr);
		return NULL;
	}
	LIST_INSERT_HEAD(&rumprunners, rr, rr_entries);

	/* async launch? */
	if ((flags & (RUMPRUN_EXEC_BACKGROUND | RUMPRUN_EXEC_PIPE)) != 0) {
		return rr;
	}

	pthread_mutex_lock(&w_mtx);
	while ((rr->rr_flags & (RUMPRUNNER_DONE|RUMPRUNNER_DAEMON)) == 0) {
		pthread_cond_wait(&w_cv, &w_mtx);
	}
	pthread_mutex_unlock(&w_mtx);

	if (rr->rr_flags & RUMPRUNNER_DONE) {
		rumprun_wait(rr);
		rr = NULL;
	}
	return rr;
}

int
rumprun_wait(void *cookie)
{
	struct rumprunner *rr = cookie;
	void *retval;

	pthread_join(rr->rr_mainthread, &retval);
	LIST_REMOVE(rr, rr_entries);
	free(rr);

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
		if (rr->rr_flags & RUMPRUNNER_DONE) {
			break;
		}
	}
	pthread_mutex_unlock(&w_mtx);
	assert(rr);

	return rr;
}

/*
 * Detaches current program.  Must always be called from
 * the main thread of an application.  That's fine, since
 * given that the counterpart on a regular system (daemon()) forks,
 * it too must be called before threads are taken into use.
 *
 * It is expected that POSIX programs call this routine via daemon().
 */
void
rumprun_daemon(void)
{
	struct rumprunner *rr;

	LIST_FOREACH(rr, &rumprunners, rr_entries) {
		if (rr->rr_mainthread == pthread_self())
			break;
	}
	assert(rr);

	pthread_mutex_lock(&w_mtx);
	rr->rr_flags |= RUMPRUNNER_DAEMON;
	pthread_cond_broadcast(&w_cv);
	pthread_mutex_unlock(&w_mtx);
}

/*
 * This is a simplified interface for writing sysctl(7) values, designed for
 * ease of use on data parsed by rumprun_config(). The 'value' passed will be
 * converted to the data type of the sysctl variable 'key' obtained from
 * sysctlgetmibinfo() and set using sysctl() if the conversion is successful.
 *
 * Special handling is provided for setting 'proc.curproc.rlimit.*.*' where a
 * 'value' of "unlimited" is interpreted as RLIM_INFINITY.
 *
 * Returns 0 on success, or any of the errno values documented in sysctl(3) on
 * error, and additionally:
 *
 * ERANGE: The specified 'value' is out of range for the data type of 'key'.
 * EINVAL: The specified 'value' is invalid for the data type of 'key' (e.g.
 * 'key' is of type CTLTYPE_INT but 'value' is not a number).
 */
int
rumprun_sysctlw(char *key, char *value)
{
	int name[CTL_MAXNAME];
	u_int namelen = CTL_MAXNAME;
	struct sysctlnode *node = NULL;

	/*
	 * Check if key is a valid node. We want name and namelen to pass to
	 * sysctl() later, and node to inspect the node type. Don't care about
	 * the canonical name.
	 */
	if (sysctlgetmibinfo(key, &name[0], &namelen, NULL, NULL, &node,
			SYSCTL_VERSION) == -1) {
		return errno;
	}

	size_t vsz;
	void *vp;
	u_quad_t vq;
	u_int vi;
	bool vb;
	char *ep;

	switch (SYSCTL_TYPE(node->sysctl_flags)) {
	case CTLTYPE_INT:
	case CTLTYPE_BOOL:
	case CTLTYPE_QUAD:
		if ((strncmp(key, "proc.curproc.rlimit.", 20) == 0) &&
			(strcmp(value, "unlimited") == 0)) {
			vq = RLIM_INFINITY;
		}
		else {
			errno = 0;
			vq = strtouq(value, &ep, 0);
			if (vq == UQUAD_MAX && errno == ERANGE) {
				free(node);
				return ERANGE;
			}
			if (ep == value || *ep != '\0') {
				free(node);
				return EINVAL;
			}
		}
		switch (SYSCTL_TYPE(node->sysctl_flags)) {
		case CTLTYPE_INT:
			vi = (u_int)(vq >> 32);
			if (vi != (u_int)-1 && vi != 0) {
				free(node);
				return ERANGE;
			}
			vi = (u_int)vq;
			vp = &vi;
			vsz = sizeof(vi);
			break;
		case CTLTYPE_BOOL:
			vb = (bool)vq;
			vp = &vb;
			vsz = sizeof(vb);
			break;
		case CTLTYPE_QUAD:
			vp = &vq;
			vsz = sizeof(vq);
			break;
		default:
			assert(false);
		}
		break;
	case CTLTYPE_STRING:
		vsz = strlen(value) + 1;
		if (vsz > node->sysctl_size && node->sysctl_size != 0) {
			free(node);
			return ERANGE;
		}
		vp = value;
		break;
	default:
		free(node);
		return EINVAL;
	}

	if (sysctl(name, namelen, NULL, NULL, vp, vsz) == -1) {
		free(node);
		return errno;
	}
	free(node);
	return 0;
}

void __attribute__((noreturn))
rumprun_reboot(void)
{

	_netbsd_userlevel_fini();
	rump_sys_reboot(0, 0);

	bmk_platform_halt("reboot returned");
}
