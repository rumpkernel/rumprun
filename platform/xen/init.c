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

#include <bmk-rumpuser/core_types.h> /* XXX */

#include <mini-os/types.h>
#include <mini-os/hypervisor.h>
#include <mini-os/kernel.h>
#include <mini-os/xenbus.h>
#include <xen/xen.h>

#include <bmk-core/mainthread.h>
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>

static char *
get_xenstorecfg(void)
{
	xenbus_transaction_t txn;
	char *cfg;
	int retry;

	if (xenbus_transaction_start(&txn))
		return NULL;
	xenbus_read(txn, "rumprun/cfg", &cfg);
	/* XXX: unclear what the "retry" param is supposed to signify */
	xenbus_transaction_end(txn, 0, &retry);

	return cfg;
}

int
app_main(start_info_t *si)
{
	char *cmdline;

	/*
	 * So, what we currently do is:
	 *   1) try to fetch the config from xenstore.  if available, use that
	 *   2) else, just pass the command line to bmk_mainthread
	 *
	 * Not sure if we eventually need to pass both, but we can change
	 * it later.
	 */

	if ((cmdline = get_xenstorecfg()) == NULL)
		cmdline = (char *)si->cmd_line;

	bmk_mainthread(cmdline);
	/* NOTREACHED */

	return 0;
}
