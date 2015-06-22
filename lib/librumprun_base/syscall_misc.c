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

#include <sys/cdefs.h>
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

void __dead
_exit(int eval)
{
	if (eval) {
		printf("\n=== ERROR: _exit(%d) called ===\n", eval);
	} else {
		printf("\n=== _exit(%d) called ===\n", eval);
	}

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
