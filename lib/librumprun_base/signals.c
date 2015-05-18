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
 * Our signal strategy: these calls always return success so that
 * applications do not panic, but we never deliver signals.
 */

#include <sys/cdefs.h>
#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#define STUBWARN()							\
do {									\
  if (!warned) {							\
	warned = 1;							\
	fprintf(stderr, "rumprun: call to ``%s'' ignored\n",		\
	    __FUNCTION__);						\
  }									\
} while (/*CONSTCOND*/0)

int
sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	static int warned = 0;

	STUBWARN();

	/* should probably track contents, maybe later */
	if (oact)
		memset(oact, 0, sizeof(*oact));
	return 0;
}
__strong_alias(sigaction,__sigaction14);

int _sys___sigprocmask14(int, const sigset_t *, sigset_t *); /* XXX */
int
_sys___sigprocmask14(int how, const sigset_t *set, sigset_t *oset)
{
	static int warned = 0;

	STUBWARN();

	/* should probably track contents, maybe later */
	if (oset)
		memset(oset, 0, sizeof(*oset));
	return 0;
}
__strong_alias(sigprocmask,__sigprocmask14);
__weak_alias(__sigprocmask14,_sys___sigprocmask14);

int
sigpending(sigset_t *set)
{
	static int warned = 0;

	STUBWARN();

	if (set)
		memset(set, 0, sizeof(*set));
	return 0;
}
__strong_alias(sigpending,__sigpending14);
