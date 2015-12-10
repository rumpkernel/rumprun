/*-
 * Copyright (c) 2013, 2015 Antti Kantee.  All Rights Reserved.
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
#include <sys/queue.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bmk-core/pgalloc.h>

#ifdef RUMPRUN_MMAP_DEBUG
#define MMAP_PRINTF(x) printf x
#else
#define MMAP_PRINTF(x)
#endif

struct mmapchunk {
	void *mm_start;
	size_t mm_size;
	size_t mm_pgsleft;

	LIST_ENTRY(mmapchunk) mm_chunks;
};
/*
 * XXX: use a tree?  we don't know how many entries we get,
 * someone might mmap page individually ...
 */
static LIST_HEAD(, mmapchunk) mmc_list = LIST_HEAD_INITIALIZER(&mmc_list);

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

static void *
mmapmem_alloc(size_t roundedlen)
{
	struct mmapchunk *mc;
	void *v;
	int order;

	mc = malloc(sizeof(*mc));
	if (mc == NULL)
		return NULL;

	order = size2order(roundedlen);
	v = bmk_pgalloc(order);
	if (v == NULL) {
		free(mc);
		return NULL;
	}
	memset(v, 0, (1UL<<order) * pagesize());

	mc->mm_start = v;
	mc->mm_size = roundedlen;
	mc->mm_pgsleft = roundedlen / pagesize();

	LIST_INSERT_HEAD(&mmc_list, mc, mm_chunks);

	return v;
}

static int
mmapmem_free(void *addr, size_t roundedlen)
{
	struct mmapchunk *mc;
	size_t npgs;
	int order;

	LIST_FOREACH(mc, &mmc_list, mm_chunks) {
		if (mc->mm_start <= addr &&
		    ((uint8_t *)mc->mm_start + mc->mm_size
		      >= (uint8_t *)addr + roundedlen))
			break;
	}
	if (!mc) {
		return EINVAL;
	}

	npgs = roundedlen / pagesize();
	assert(npgs <= mc->mm_pgsleft);
	mc->mm_pgsleft -= npgs;
	if (mc->mm_pgsleft)
		return 0;

	/* no pages left => free bookkeeping chunk */
	LIST_REMOVE(mc, mm_chunks);
	order = size2order(mc->mm_size);
	bmk_pgfree(mc->mm_start, order);
	free(mc);

	return 0;
}

void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	void *v;
	ssize_t nn;
	size_t roundedlen, nnu;
	int error = 0;

	MMAP_PRINTF(("-> mmap: %p %zu, 0x%x, 0x%x, %d, %" PRId64 "\n",
	    addr, len, prot, flags, fd, off));

	if (fd != -1 && prot != PROT_READ) {
		MMAP_PRINTF(("mmap: trying to r/w map a file. failing!\n"));
		error = ENOTSUP;
		goto out;
	}

	/* we're not going to even try */
	if (flags & MAP_FIXED) {
		error = ENOMEM;
		goto out;
	}

	/* offset should be aligned to page size */
	if ((off & (pagesize()-1)) != 0) {
		error = EINVAL;
		goto out;
	}

	/* allocate full whatever-we-lie-to-be-pages */
	roundedlen = roundup2(len, pagesize());
	if ((v = mmapmem_alloc(roundedlen)) == NULL) {
		error = ENOMEM;
		goto out;
	}

	if (flags & MAP_ANON)
		goto out;

	if ((nn = pread(fd, v, roundedlen, off)) == -1) {
		MMAP_PRINTF(("mmap: failed to populate r/o file mapping!\n"));
		error = errno;
		assert(error != 0);
		mmapmem_free(v, roundedlen);
		goto out;
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

 out:
	if (error) {
		errno = error;
		v = MAP_FAILED;
	}
	MMAP_PRINTF(("<- mmap: %p %d\n", v, error));
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
	int rv;

	MMAP_PRINTF(("-> munmap: %p, %zu\n", addr, len));

	/* addr must be page-aligned */
	if (((uintptr_t)addr & (pagesize()-1)) != 0) {
		rv = EINVAL;
		goto out;
	}

	rv =  mmapmem_free(addr, roundup2(len, pagesize()));

 out:
	MMAP_PRINTF(("<- munmap: %d\n", rv));
	return rv;
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
