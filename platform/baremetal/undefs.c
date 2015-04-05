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

#include <bmk/kernel.h>

/*
 * Stubs for unused rump kernel hypercalls
 */

#define NOTHING(name) \
    int name(void); int name(void) \
    {bmk_cons_puts("unimplemented: " #name "\n"); for (;;);}

#define REALNOTHING(name) \
    int name(void); int name(void) {return 1;}

NOTHING(rumpuser_exit)

NOTHING(rumpuser_open)
NOTHING(rumpuser_close)
REALNOTHING(rumpuser_getfileinfo)
NOTHING(rumpuser_bio)

/* libc */

REALNOTHING(__sigaction14);
NOTHING(__getrusage50);
//REALNOTHING(__sigprocmask14);
REALNOTHING(sigqueueinfo);
//REALNOTHING(rasctl);
//NOTHING(_lwp_self);

//NOTHING(__libc_static_tls_setup);

NOTHING(__fork);
NOTHING(__vfork14);
NOTHING(kill);
NOTHING(getpriority);
NOTHING(setpriority);

int execve(const char *, char *const[], char *const[]);
int
execve(const char *file, char *const argv[], char *const envp[])
{

	bmk_cons_puts("execve not implemented\n");
	return -1;
}

NOTHING(_sys_mq_send);
NOTHING(_sys_mq_receive);
NOTHING(_sys___mq_timedsend50);
NOTHING(_sys___mq_timedreceive50);
NOTHING(_sys_msgrcv);
NOTHING(_sys_msgsnd);
NOTHING(_sys___msync13);
NOTHING(_sys___wait450);
NOTHING(_sys___sigsuspend14);
