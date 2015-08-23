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

#include <sys/param.h>
#include <sys/mman.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bmk-core/pgalloc.h>

#ifdef RUMPRUN_MMAP_DEBUG
#define MMAP_PRINTF(x) printf x
#else
#define MMAP_PRINTF(x)
#endif

/* XXX: need actual const macro */
static inline long __constfunc
pagesize(void)
{

	return sysconf(_SC_PAGESIZE);
}

/*
 * calculate the order we need for pgalloc()
 */
static int
size2order(size_t wantedsize)
{
	int npgs = wantedsize / pagesize();
	int powtwo;

	powtwo = 8*sizeof(npgs) - __builtin_clz(npgs);
	if ((npgs & (npgs-1)) == 0)
		powtwo--;

	return powtwo;
}

void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	void *v;
	ssize_t nn;
	size_t roundedlen, nnu;
	int order;
	int error;

	if (fd != -1 && prot != PROT_READ) {
		MMAP_PRINTF(("mmap: trying to r/w map a file. failing!\n"));
		errno = ENOTSUP;
		return MAP_FAILED;
	}

	/* we're not going to even try */
	if (flags & MAP_FIXED) {
		errno = ENOMEM;
		return MAP_FAILED;
	}

	/* offset should be aligned to page size */
	if ((off & (pagesize()-1)) != 0) {
		errno = EINVAL;
		return MAP_FAILED;
	}

	/* allocate full whatever-we-lie-to-be-pages */
	roundedlen = roundup2(len, pagesize());
	order = size2order(roundedlen);
	if ((v = (void *)bmk_pgalloc(order)) == NULL) {
		errno = ENOMEM;
		return MAP_FAILED;
	}

	if (flags & MAP_ANON)
		return v;

	if ((nn = pread(fd, v, roundedlen, off)) == -1) {
		MMAP_PRINTF(("mmap: failed to populate r/o file mapping!\n"));
		error = errno;
		bmk_pgfree(v, order);
		errno = error;
		return MAP_FAILED;
	}
	nnu = (size_t)nn;

	/*
	 * Memory after the end of the object until the end of the page
	 * should be 0-filled.  We don't really know when the object
	 * stops (we could do a fstat(), but that's racy), so just assume
	 * that the caller knows what her or she is doing.
	 */
	if (nnu != roundedlen) {
		assert(nnu < roundedlen);
		memset((uint8_t *)v+nnu, 0, roundedlen-nnu);
	}

	return v;
}
#undef mmap
__weak_alias(mmap,_mmap);

int _sys___msync13(void *, size_t, int);
int
_sys___msync13(void *addr, size_t len, int flags)
{

	/* catch a few easy errors */
	if (((uintptr_t)addr & (pagesize()-1)) != 0)
		return EINVAL;
	if ((flags & (MS_SYNC|MS_ASYNC)) == (MS_SYNC|MS_ASYNC))
		return EINVAL;

	/* otherwise just pretend that we are the champions my friend */
	return 0;
}

int
munmap(void *addr, size_t len)
{
	int order;

	/* addr must be page-aligned */
	if (((uintptr_t)addr & (pagesize()-1)) != 0)
		return EINVAL;

	order = size2order(roundup2(len, pagesize()));
	bmk_pgfree(addr, order);

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
