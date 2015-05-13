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
 * This is the guest side of a simple test framework, which just
 * runs a test and writes the results to a given filename.  More
 * often than not, the filename is a virtual disk device.
 *
 * This code runs in the application space.
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rumprun/tester.h>

#define INITIAL "??   0\n"

static const char *trydisk[] = {
	"/dev/ld0d",
	"/dev/xenblk0",
};
static int logfd;
static int logrv = 1;

static void
logexit(void)
{
	char buf[sizeof(INITIAL)];

	snprintf(buf, sizeof(buf), "%s % 3d\n", logrv == 0 ? "OK" : "NO",logrv);
	pwrite(logfd, buf, sizeof(INITIAL)-1, 0);
	fflush(stdout);
	close(logfd);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	sync();
}

int
main(int argc, char *argv[])
{
	unsigned int i;

	/* were we ran via the test framework? */
	if (argc < 2 || strcmp(argv[1], "__test") != 0)
		return rumprun_test(argc, argv);

	/*
	 * XXX: need a better way to determine disk device!
	 * e.g. rumpconfig (which has currently not been
	 * implemented for baremetal)
	 */
	for (i = 0; i < __arraycount(trydisk); i++) {
		logfd = open(trydisk[i], O_RDWR);
		if (logfd != -1)
			break;
	}
	if (logfd == -1) {
		err(1, "rumprun_test: unable to open data device");
	}

	if (write(logfd, INITIAL, sizeof(INITIAL)-1) != sizeof(INITIAL)-1) {
		err(1, "rumprun_test: initial write failed\n");
	}

	if (dup2(logfd, STDOUT_FILENO) == -1)
		err(1, "rumprun_test: dup2 to stdout");
	if (dup2(logfd, STDERR_FILENO) == -1)
		err(1, "rumprun_test: dup2 to stdout");

	/*
	 * Run the actual test.  This would of course be much nicer if
	 * we had the ability to do something like dlsym(RTLD_NEXT),
	 * but we don't for now, so let's not try about it.
	 */
	printf("=== FOE RUMPRUN 12345 TES-TER 54321 ===\n");
	atexit(logexit);
	logrv = rumprun_test(argc+1, argv-1);
	printf("=== RUMPRUN 12345 TES-TER 54321 EOF ===\n");

	exit(logrv);
}
