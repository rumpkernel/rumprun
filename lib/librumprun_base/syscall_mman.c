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

/*
 * Emulate a bit of memory management syscalls.  Most are nops due
 * to the fact that there is virtual memory.  Things like mprotect()
 * don't work 100% correctly, but we can't really do anything about it,
 * so we lie to the caller and cross our fingers.
 */

/* for libc namespace */
#define mmap _mmap

#include <sys/cdefs.h>
#include <sys/mman.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef RUMPRUN_MMAP_DEBUG
#define MMAP_PRINTF(x) printf x
#else
#define MMAP_PRINTF(x)
#endif

void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	void *v;
	ssize_t nn;
	long pagesize = sysconf(_SC_PAGESIZE);
	int error;

	if (fd != -1 && prot != PROT_READ) {
		MMAP_PRINTF(("mmap: trying to r/w map a file. failing!\n"));
		errno = ENOTSUP;
		return MAP_FAILED;
	}

	if ((error = posix_memalign(&v, pagesize, len)) != 0) {
		errno = error;
		return MAP_FAILED;
	}

	if (flags & MAP_ANON)
		return v;

	if ((nn = pread(fd, v, len, off)) == -1) {
		MMAP_PRINTF(("mmap: failed to populate r/o file mapping!\n"));
		error = errno;
		free(v);
		errno = error;
		return MAP_FAILED;
	}
	return v;
}
#undef mmap
__weak_alias(mmap,_mmap);

int _sys___msync13(void *, size_t, int);
int
_sys___msync13(void *addr, size_t len, int flags)
{
	long pagesize = sysconf(_SC_PAGESIZE);

	/* catch a few easy errors */
	if (((uintptr_t)addr & (pagesize-1)) != 0)
		return EINVAL;
	if ((flags & (MS_SYNC|MS_ASYNC)) == (MS_SYNC|MS_ASYNC))
		return EINVAL;

	/* otherwise just pretend that we are the champions my friend */
	return 0;
}

int
munmap(void *addr, size_t len)
{

	free(addr);
	return 0;
}

int
madvise(void *addr, size_t len, int adv)
{

	return 0;
}

int
mprotect(void *addr, size_t len, int prot)
{

	return 0;
}

int
minherit(void *addr, size_t len, int inherit)
{

	return 0;
}

int
mlockall(int flags)
{

	return 0;
}

int
munlockall(void)
{

	return 0;
}

int
mlock(const void *addr, size_t len)
{

	return 0;
}

int
munlock(const void *addr, size_t len)
{

	return 0;
}

int
mincore(void *addr, size_t length, char *vec)
{
	long page_size = sysconf(_SC_PAGESIZE);

	memset(vec, 0x01, (length + page_size - 1) / page_size);
	return 0;
}
