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

/* XXX: incorrect return value, need to fix other things first */
#define STUB_ERRNO(name)				\
  int name(void); int name(void) {			\
	static int done = 0;				\
	errno = ENOTSUP;				\
	if (done) return ENOTSUP; done = 1;		\
	fprintf(stderr, "STUB ``%s'' called\n", #name);	\
	return ENOTSUP;}

#define STUB_SILENT_IGNORE(name)			\
  int name(void); int name(void) {			\
	static int done = 0;				\
	errno = 0;					\
	if (done) return 0;				\
	done = 1;					\
	fprintf(stderr, "``%s'' ignored\n", #name);	\
	return 0;}

#define STUB_RETURN(name)				\
  int name(void); int name(void) {			\
	static int done = 0;				\
	if (done) return ENOTSUP; done = 1;		\
	fprintf(stderr, "STUB ``%s'' called\n", #name);	\
	return ENOTSUP;}

STUB_SILENT_IGNORE(__sigaction_sigtramp);

STUB_RETURN(posix_spawn);

STUB_ERRNO(__fork);
STUB_ERRNO(__vfork14);
STUB_ERRNO(kill);
STUB_ERRNO(getpriority);
STUB_ERRNO(setpriority);

/* for pthread_cancelstub */
STUB_ERRNO(_sys_mq_send);
STUB_ERRNO(_sys_mq_receive);
STUB_ERRNO(_sys___mq_timedsend50);
STUB_ERRNO(_sys___mq_timedreceive50);
STUB_ERRNO(_sys_msgrcv);
STUB_ERRNO(_sys_msgsnd);
STUB_ERRNO(_sys___msync13);
STUB_ERRNO(_sys___wait450);
STUB_ERRNO(_sys___sigsuspend14);

/* execve is open-coded to match the prototype to avoid a compiler warning */
int execve(const char *, char *const[], char *const[]);
int
execve(const char *file, char *const argv[], char *const envp[])
{

	fprintf(stderr, "execve not implemented\n");
	errno = ENOTSUP;
	return -1;
}
