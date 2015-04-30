/*-
 * Copyright (c) 2014 Citrix.  All Rights Reserved.
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

#include <mini-os/types.h>
#include <mini-os/hypervisor.h>
#include <mini-os/kernel.h>
#include <xen/xen.h>

#include <bmk-core/memalloc.h>

#include <rumprun-base/rumprun.h>
#include <rumprun-base/parseargs.h>

int
app_main(start_info_t *si)
{
	char argv0[] = "rumprun-xen";
	char *rawcmdline = (char *)si->cmd_line;
	int nargs;
	char **argv;
	void *cookie;

	rumprun_parseargs(rawcmdline, &nargs, 0);
	argv = bmk_xmalloc(sizeof(*argv) * (nargs+3));
	argv[0] = argv0;
	rumprun_parseargs(rawcmdline, &nargs, argv+1);
	argv[nargs+1] = 0;
	argv[nargs+2] = 0;

	rumprun_boot(NULL); /* Xen doesn't use cmdline the same way (yet?) */

	cookie = rumprun(main, nargs+1, argv);
	rumprun_wait(cookie);

	rumprun_reboot();
}

/*
 * Local variables:
 *  c-file-style: "linux"
 *  indent-tabs-mode: t
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
