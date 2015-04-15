/*-
 * Copyright (c) 2014 Antti Kantee.  All Rights Reserved.
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

#include <errno.h>
#include <stdio.h>

#define STUB(name)				\
  int name(void); int name(void) {		\
	static int done = 0;			\
	errno = ENOTSUP;			\
	if (done) return ENOTSUP; done = 1;	\
	printf("STUB ``%s'' called\n", #name);	\
	return ENOTSUP;}

STUB(__sigaction14);
STUB(__sigaction_sigtramp);
STUB(sigaction);
STUB(sigprocmask);
STUB(__getrusage50);

STUB(__fork);
STUB(__vfork14);
STUB(kill);
STUB(getpriority);
STUB(setpriority);
STUB(posix_spawn);

STUB(mlockall);

/* for pthread_cancelstub */
STUB(_sys_mq_send);
STUB(_sys_mq_receive);
STUB(_sys___mq_timedsend50);
STUB(_sys___mq_timedreceive50);
STUB(_sys_msgrcv);
STUB(_sys_msgsnd);
STUB(_sys___msync13);
STUB(_sys___wait450);
STUB(_sys___sigsuspend14);

int execve(const char *, char *const[], char *const[]);
int
execve(const char *file, char *const argv[], char *const envp[])
{

	printf("execve not implemented\n");
	errno = ENOTSUP;
	return -1;
}
