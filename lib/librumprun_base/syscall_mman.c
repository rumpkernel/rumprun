/*-
 * Copyright (c) 2016 Antti Kantee.  All Rights Reserved.
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
 * Syscall wrappers for memory management routines (not provided by
 * standard rump kernels).  These are handwritten libc-level wrappers.
 * We should maybe try to autogenerate them some fine day ...
 */

#define mmap _mmap

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>

#include <errno.h>
#include <string.h>

/* XXX */
int	rump_syscall(int, void *, size_t, register_t *);

#if     BYTE_ORDER == BIG_ENDIAN
#define SPARG(p,k)      ((p)->k.be.datum)
#else /* LITTLE_ENDIAN, I hope dearly */
#define SPARG(p,k)      ((p)->k.le.datum)
#endif

void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t pos)
{
	struct sys_mmap_args callarg;
	register_t retval[2];
	int error;

	memset(&callarg, 0, sizeof(callarg));
	SPARG(&callarg, addr) = addr;
	SPARG(&callarg, len) = len;
	SPARG(&callarg, prot) = prot;
	SPARG(&callarg, flags) = flags;
	SPARG(&callarg, fd) = fd;
	SPARG(&callarg, pos) = pos;

	error = rump_syscall(SYS_mmap, &callarg, sizeof(callarg), retval);
	errno = error;
	if (error == 0) {
		return (void *)retval[0];
	}
	return MAP_FAILED;
}
#undef mmap
__weak_alias(mmap,_mmap);

int
munmap(void *addr, size_t len)
{
	struct sys_munmap_args callarg;
	register_t retval[2];
	int error;

	memset(&callarg, 0, sizeof(callarg));
	SPARG(&callarg, addr) = addr;
	SPARG(&callarg, len) = len;

	error = rump_syscall(SYS_munmap, &callarg, sizeof(callarg), retval);
	errno = error;
	if (error == 0) {
		return (int)retval[0];
	}
	return -1;
}

int _sys___msync13(void *, size_t, int);
int
_sys___msync13(void *addr, size_t len, int flags)
{
	struct sys___msync13_args callarg;
	register_t retval[2];
	int error;

	memset(&callarg, 0, sizeof(callarg));
	SPARG(&callarg, addr) = addr;
	SPARG(&callarg, len) = len;

	error = rump_syscall(SYS___msync13, &callarg, sizeof(callarg), retval);
	errno = error;
	if (error == 0) {
		return 0;
	}
	return -1;
}
__weak_alias(___msync13,_sys___msync13);
__weak_alias(__msync13,_sys___msync13);

int
mincore(void *addr, size_t len, char *vec)
{
	struct sys_mincore_args callarg;
	register_t retval[2];
	int error;

	memset(&callarg, 0, sizeof(callarg));
	SPARG(&callarg, addr) = addr;
	SPARG(&callarg, len) = len;
	SPARG(&callarg, vec) = vec;

	error = rump_syscall(SYS_mincore, &callarg, sizeof(callarg), retval);
	errno = error;
	if (error == 0) {
		return 0;
	}
	return -1;
}

/*
 * We "know" that the following are stubs also in the kernel.  Risk of
 * them going out-of-sync is quite minimal ...
 */

int
madvise(void *addr, size_t len, int adv)
{

	return 0;
}
__strong_alias(mprotect,madvise);
__strong_alias(minherit,madvise);
__strong_alias(mlock,madvise);
__strong_alias(mlockall,madvise);
__strong_alias(munlock,madvise);
__strong_alias(munlockall,madvise);
