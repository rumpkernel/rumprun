/*-
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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

#include <bmk-core/errno.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/null.h>
#include <bmk-core/platform.h>
#include <bmk-core/sched.h>
#include <bmk-core/string.h>

#include <bmk-rumpuser/core_types.h>
#include <bmk-rumpuser/rumpuser.h>

int
rumpuser_clock_gettime(int which, int64_t *sec, long *nsec)
{
	bmk_time_t time;

	time = bmk_platform_cpu_clock_monotonic();

	switch (which) {
	case RUMPUSER_CLOCK_RELWALL:
		time += bmk_platform_cpu_clock_epochoffset();
		break;
	case RUMPUSER_CLOCK_ABSMONO:
		break;
	}

	*sec  = time / (1000*1000*1000ULL);
	*nsec = time % (1000*1000*1000ULL);

	return 0;
}

int
rumpuser_clock_sleep(int enum_rumpclock, int64_t sec, long nsec)
{
	enum rumpclock rclk = enum_rumpclock;
	bmk_time_t deadline = 0;
	int nlocks;

	rumpkern_unsched(&nlocks, NULL);
	switch (rclk) {
	case RUMPUSER_CLOCK_RELWALL:
		deadline = bmk_platform_cpu_clock_monotonic();
		break;
	case RUMPUSER_CLOCK_ABSMONO:
		break;
	}
	deadline += sec * 1000*1000*1000 + nsec;
	bmk_sched_blockprepare_timeout(deadline);
	bmk_sched_block();
	rumpkern_sched(nlocks, NULL);

	return 0;
}
