/*-
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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
 * Emulate a bit of mmap.  Currently just MAP_ANON
 * and MAP_FILE+PROT_READ are supported.  For files, it's not true
 * mmap, but should cover a good deal of the cases anyway.
 */

/* for libc namespace */
#define mmap _mmap

#include <sys/cdefs.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <lwp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <bmk-core/core.h>
#include <bmk-core/sched.h>

#include <rump/rump.h>

#ifdef RUMPRUN_MMAP_DEBUG
#define MMAP_PRINTF(x) printf x
#else
#define MMAP_PRINTF(x)
#endif

void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	void *v;
	ssize_t nn;
	int error;

	if (fd != -1 && prot != PROT_READ) {
		MMAP_PRINTF(("mmap: trying to r/w map a file. failing!\n"));
		errno = ENOTSUP;
		return MAP_FAILED;
	}

	if ((error = posix_memalign(&v, bmk_pagesize, len)) != 0) {
		errno = error;
		return MAP_FAILED;
	}

	if (flags & MAP_ANON)
		return v;

	if ((nn = pread(fd, v, len, off)) == -1) {
		MMAP_PRINTF(("mmap: failed to populate r/o file mapping!\n"));
		error = errno;
		free(v);
		errno = error;
		return MAP_FAILED;
	}
	return v;
}
#undef mmap
__weak_alias(mmap,_mmap);

int
madvise(void *addr, size_t len, int adv)
{

	/* thanks for the advise, pal */
	return 0;
}

int
mprotect(void *addr, size_t len, int prot)
{
	/* no protection */
	return 0;
}

int
minherit(void *addr, size_t len, int inherit)
{
	/* nothing to inherit */
	return 0;
}

int
mlockall(int flags)
{

	/* no vm => everything is automatically locked */
	return 0;
}

int
munmap(void *addr, size_t len)
{

	free(addr);
	return 0;
}

void __dead
_exit(int eval)
{
	if (eval) {
		printf("\n=== ERROR: _exit(%d) called ===\n", eval);
	} else {
		printf("\n=== _exit(%d) called ===\n", eval);
	}

	rump_pub_lwproc_releaselwp();
	pthread_exit((void *)(uintptr_t)eval);
}

/* XXX: manual proto.  plug into libc internals some other day */
int     ____sigtimedwait50(const sigset_t * __restrict,
    siginfo_t * __restrict, struct timespec * __restrict);
int
____sigtimedwait50(const sigset_t *set, siginfo_t *info,
	struct timespec *timeout)
{
	int rc;

	rc = _lwp_park(CLOCK_MONOTONIC, 0, timeout, 0, NULL, NULL);
	if (rc == -1) {
		if (errno == ETIMEDOUT)
			errno = EAGAIN;
	} else {
		errno = EAGAIN;
	}

	return -1;
}

/* XXX: manual proto.  plug into libc internals some other day */
int __getrusage50(int, struct rusage *);
int
__getrusage50(int who, struct rusage *usage)
{

	/* We fake something.  We should query the scheduler some day */
	memset(usage, 0, sizeof(*usage));
	if (who == RUSAGE_SELF) {
		usage->ru_utime.tv_sec = 1;
		usage->ru_utime.tv_usec = 1;
		usage->ru_stime.tv_sec = 1;
		usage->ru_stime.tv_usec = 1;
	}

	usage->ru_maxrss = 1024;
	usage->ru_ixrss = 1024;
	usage->ru_idrss = 1024;
	usage->ru_isrss = 1024;

	usage->ru_nvcsw = 1;
	usage->ru_nivcsw = 1;

	/* XXX: wrong in many ways */
	return ENOTSUP;
}
