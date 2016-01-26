/*
 * Copyright (c) 2014 Martin Lucina.  All Rights Reserved.
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

#ifndef _BMKCOMMON_RUMPRUN_CONFIG_H_
#define _BMKCOMMON_RUMPRUN_CONFIG_H_

#include <sys/queue.h>

/* yeah, simple */
#define RUMPRUN_DEFAULTUSERSTACK ((32*(sizeof(void *)/4)*4096)/1024)

void	rumprun_config(char *);

typedef int rre_mainfn(int, char *[]);

#define RUMPRUN_EXEC_BACKGROUND 0x01
#define RUMPRUN_EXEC_PIPE	0x02
struct rumprun_exec {
	int rre_flags;

	TAILQ_ENTRY(rumprun_exec) rre_entries;

	int rre_argc;
	rre_mainfn *rre_main;
	char *rre_argv[];
};

TAILQ_HEAD(rumprun_execs, rumprun_exec);
extern struct rumprun_execs rumprun_execs;

/*
 * XXX: The definition of this structure needs to be kept in sync with that in
 * rumprun-bake, which generates rumprun_bins[] during baking.
 */
struct rumprun_bin {
	const char *binname;
	rre_mainfn *main;
};

extern struct rumprun_bin *rumprun_bins[];

#endif /* _BMKCOMMON_RUMPRUN_CONFIG_H_ */
