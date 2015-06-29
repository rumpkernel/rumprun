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

#ifndef _RUMPRUN_BASE_RUMPRUN_H_
#define _RUMPRUN_BASE_RUMPRUN_H_

typedef int mainlike_fn(int, char *[]);
mainlike_fn main;

/*
 * for baking multiple executables into a single binary
 * XXX: should not depend on explicit symbol names with hardcoded
 * limits
 */
mainlike_fn rumprun_notmain;
mainlike_fn rumpbake_main1;
mainlike_fn rumpbake_main2;
mainlike_fn rumpbake_main3;
mainlike_fn rumpbake_main4;
mainlike_fn rumpbake_main5;
mainlike_fn rumpbake_main6;
mainlike_fn rumpbake_main7;
mainlike_fn rumpbake_main8;

#define RUNMAIN(i)							\
	if (rumpbake_main##i == rumprun_notmain)			\
		break;							\
	rumprun(rumpbake_main##i,					\
	    rumprun_cmdline_argc, rumprun_cmdline_argv);

#define RUNMAINS()							\
	do {								\
		RUNMAIN(1);						\
		RUNMAIN(2);						\
		RUNMAIN(3);						\
		RUNMAIN(4);						\
		RUNMAIN(5);						\
		RUNMAIN(6);						\
		RUNMAIN(7);						\
		RUNMAIN(8);						\
	} while (/*CONSTCOND*/0)

void	rumprun_boot(char *);

void *	rumprun(mainlike_fn, int, char *[]);
int	rumprun_wait(void *);
void *	rumprun_get_finished(void);

void	rumprun_reboot(void) __dead;

/* XXX: this prototype shouldn't be here (if it should exist at all) */
void	rumprun_daemon(void);

#endif /* _RUMPRUN_BASE_RUMPRUN_H_ */
