/*-
 * Copyright (c) 2016 Antti Kantee <pooka@fixup.fi>
 * All Rights Reserved.
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

#include <sys/types.h>

#include <bmk-core/core.h>
#include <bmk-core/sched.h>

#include <rump/rump.h>
#include <rump/rumpfs.h>
#include <rump/rump_syscalls.h>

#include <rumprun/platefs.h>

void
rumprun_platefs(const char **dirs, size_t ndirs,
	struct rumprun_extfile *refs, size_t nrefs)
{
	unsigned i;
	int fd;

	for (i = 0; i < ndirs; i++) {
		if (rump_sys_mkdir(dirs[i], 0777) == -1) {
			if (*bmk_sched_geterrno() != RUMP_EEXIST)
				bmk_platform_halt("platefs: mkdir");
		}
	}


	for (i = 0; i < nrefs; i++) {
		if ((fd = rump_sys_open(refs[i].ref_fname,
		    RUMP_O_CREAT | RUMP_O_RDWR | RUMP_O_EXCL, 0777)) == -1)
			bmk_platform_halt("platefs: open");
		if (rump_sys_fcntl(fd, RUMPFS_FCNTL_EXTSTORAGE_ADD,
		    &refs[i].ref_es) == -1)
			bmk_platform_halt("platefs: fcntl");
		rump_sys_close(fd);
	}
}
