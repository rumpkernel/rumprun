/*
 * Copyright (c) 2015 Martin Lucina.  All Rights Reserved.
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
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/timetc.h>

#include <mini-os/time.h>

MODULE(MODULE_CLASS_MISC, rumpxen_tc, NULL);

static u_int
rumpxen_tc_get(struct timecounter *tc)
{

	return (u_int)minios_clock_monotonic();
}

static struct timecounter rumpxen_tc = {
	.tc_get_timecount	= rumpxen_tc_get,
	.tc_poll_pps 		= NULL,
	.tc_counter_mask	= ~0,
	.tc_frequency		= 1000000000ULL,
	.tc_name		= "rumpxen",
	.tc_quality		= 100,
};

static int
rumpxen_tc_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		tc_init(&rumpxen_tc);
		break;

	case MODULE_CMD_FINI:
		tc_detach(&rumpxen_tc);
		break;

	default:
		return ENOTTY;
	}

	return 0;
}
