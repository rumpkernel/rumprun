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

#include <mini-os/types.h>
#include <mini-os/console.h>
#include <mini-os/time.h>

#include <xen/io/console.h>

#include <bmk-core/errno.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/sched.h>
#include <bmk-core/string.h>

#include <bmk-rumpuser/rumpuser.h>

int
rumpuser_getparam(const char *name, void *buf, size_t buflen)
{
	int rv = 0;

	if (buflen <= 1)
		return BMK_EINVAL;

	if (bmk_strcmp(name, RUMPUSER_PARAM_NCPU) == 0
	    || bmk_strcmp(name, "RUMP_VERBOSE") == 0) {
		bmk_strncpy(buf, "1", buflen-1);

	} else if (bmk_strcmp(name, RUMPUSER_PARAM_HOSTNAME) == 0) {
		bmk_strncpy(buf, "rump-xen", buflen-1);

	/* for memlimit, we have to implement int -> string ... */
	} else if (bmk_strcmp(name, "RUMP_MEMLIMIT") == 0) {
		unsigned long long memsize;
		char tmp[64];
		char *res = buf;
		unsigned i, j;

		/* use up to 50% memory for rump kernel */
		memsize = minios_get_memsize() / 2;
		if (memsize < (8 * 1024 * 1024)) {
			minios_printk("rumphyper: warning: low on physical "
				      "memory (%llu bytes), "
				      "suggest increasing domU allocation\n",
				      memsize);
			memsize = 8 * 1024 * 1024;
		}

		for (i = 0; memsize > 0; i++) {
			bmk_assert(i < sizeof(tmp)-1);
			tmp[i] = (memsize % 10) + '0';
			memsize = memsize / 10;
		}
		if (i >= buflen) {
			rv = 1;
		} else {
			res[i] = '\0';
			for (j = i; i > 0; i--) {
				res[j-i] = tmp[i-1];
			}
		}

	} else {
		rv = BMK_ENOENT;
	}

	return rv;
}

/* Not very random */
int
rumpuser_getrandom(void *buf, size_t buflen, int flags, size_t *retp)
{
	uint8_t *rndbuf;

	for (*retp = 0, rndbuf = buf; *retp < buflen; (*retp)++) {
		*rndbuf++ = minios_clock_monotonic() & 0xff;
	}

	return 0;
}
