/*-
 * Copyright (c) 2013, 2015, 2016 Antti Kantee.  All Rights Reserved.
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
 * Memory management syscall implementations.  These are mostly ~nops,
 * apart from mmap, which we sort of attempt to emulate since many
 * programs reserve memory using mmap instead of malloc.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>

#include "rump_private.h"

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
 * someone might mmap pages individually ...
 */
static LIST_HEAD(, mmapchunk) mmc_list = LIST_HEAD_INITIALIZER(&mmc_list);

static void *
mmapmem_alloc(size_t roundedlen)
{
	struct mmapchunk *mc;
	void *v;

	mc = kmem_alloc(sizeof(*mc), KM_SLEEP);
	if (mc == NULL)
		return NULL;

	v = rump_hypermalloc(roundedlen, PAGE_SIZE, true, "mmapmem");

	mc->mm_start = v;
	mc->mm_size = roundedlen;
	mc->mm_pgsleft = roundedlen / PAGE_SIZE;

	LIST_INSERT_HEAD(&mmc_list, mc, mm_chunks);

	return v;
}

static int
mmapmem_free(void *addr, size_t roundedlen)
{
	struct mmapchunk *mc;
	size_t npgs;

	LIST_FOREACH(mc, &mmc_list, mm_chunks) {
		if (mc->mm_start <= addr &&
		    ((uint8_t *)mc->mm_start + mc->mm_size
		      >= (uint8_t *)addr + roundedlen))
			break;
	}
	if (!mc) {
		return EINVAL;
	}

	npgs = roundedlen / PAGE_SIZE;
	KASSERT(npgs <= mc->mm_pgsleft);
	mc->mm_pgsleft -= npgs;
	if (mc->mm_pgsleft)
		return 0;

	/* no pages left => free bookkeeping chunk */
	LIST_REMOVE(mc, mm_chunks);
	kmem_free(mc->mm_start, mc->mm_size);
	kmem_free(mc, sizeof(*mc));

	return 0;
}

int
sys_mmap(struct lwp *l, const struct sys_mmap_args *uap, register_t *retval)
{
	size_t len = SCARG(uap, len);
	int prot = SCARG(uap, prot);
	int flags = SCARG(uap, flags);
	int fd = SCARG(uap, fd);
	off_t pos = SCARG(uap, pos);
	struct file *fp;
	register_t cnt;
	void *v;
	size_t roundedlen;
	int error = 0;

	MMAP_PRINTF(("-> mmap: %p %zu, 0x%x, 0x%x, %d, %" PRId64 "\n",
	    SCARG(uap, addr), len, prot, flags, fd, pos));

	if (fd != -1 && prot != PROT_READ) {
		MMAP_PRINTF(("mmap: trying to r/w map a file. failing!\n"));
		return EOPNOTSUPP;
	}

	/* we're not going to even try */
	if (flags & MAP_FIXED) {
		return ENOMEM;
	}

	/* offset should be aligned to page size */
	if ((pos & (PAGE_SIZE-1)) != 0) {
		return EINVAL;
	}

	/* allocate full whatever-we-lie-to-be-pages */
	roundedlen = roundup2(len, PAGE_SIZE);
	if ((v = mmapmem_alloc(roundedlen)) == NULL) {
		return ENOMEM;
	}

	*retval = (register_t)v;

	if (flags & MAP_ANON) {
		memset(v, 0, roundedlen);
		return 0;
	}

	/*
	 * Ok, so we have a file-backed mapping case.
	 */

	if ((fp = fd_getfile(fd)) == NULL)
		return EBADF;

	if ((fp->f_flag & FREAD) == 0) {
		fd_putfile(fd);
		return EBADF;
	}
	if (fp->f_type != DTYPE_VNODE) {
		fd_putfile(fd);
		return ENODEV;
	}

	error = dofileread(fd, fp, v, roundedlen, &pos, 0, &cnt);
	if (error) {
		mmapmem_free(v, roundedlen);
		return error;
	}

	/*
	 * Memory after the end of the object until the end of the page
	 * should be 0-filled.  We don't really know when the object
	 * stops (we could do a fstat(), but that's racy), so just assume
	 * that the caller knows what her or she is doing.
	 */
	if ((size_t)cnt != roundedlen) {
		KASSERT(cnt < roundedlen);
		memset((uint8_t *)v+cnt, 0, roundedlen-cnt);
	}

	MMAP_PRINTF(("<- mmap: %p %d\n", v, error));
	return error;
}

int
sys___msync13(struct lwp *l, const struct sys___msync13_args *uap,
	register_t *retval)
{
	void *addr = SCARG(uap, addr);
	int flags = SCARG(uap, flags);

	/* catch a few easy errors */
	if (((uintptr_t)addr & (PAGE_SIZE-1)) != 0)
		return EINVAL;
	if ((flags & (MS_SYNC|MS_ASYNC)) == (MS_SYNC|MS_ASYNC))
		return EINVAL;

	/* otherwise just pretend that we are the champions my friend */
	return 0;
}

int
sys_munmap(struct lwp *l, const struct sys_munmap_args *uap, register_t *retval)
{
	void *addr = SCARG(uap, addr);
	size_t len = SCARG(uap, len);
	int rv;

	MMAP_PRINTF(("-> munmap: %p, %zu\n", addr, len));

	/* addr must be page-aligned */
	if (((uintptr_t)addr & (PAGE_SIZE-1)) != 0) {
		rv = EINVAL;
		goto out;
	}

	rv =  mmapmem_free(addr, roundup2(len, PAGE_SIZE));

 out:
	MMAP_PRINTF(("<- munmap: %d\n", rv));
	return rv;
}

int
sys_mincore(struct lwp *l, const struct sys_mincore_args *uap,
	register_t *retval)
{
	size_t len = SCARG(uap, len);
	char *vec = SCARG(uap, vec);

	/*
	 * Questionable if we should allocate vec + copyout().
	 * Guess that's the problem of the person why copypastes
	 * this code into the wrong place.
	 */
	memset(vec, 0x01, (len + PAGE_SIZE - 1) / PAGE_SIZE);
	return 0;
}

/*
 * Rest are stubs.
 */

int
sys_madvise(struct lwp *l, const struct sys_madvise_args *uap,
	register_t *retval)
{

	return 0;
}

__strong_alias(sys_mprotect,sys_madvise);
__strong_alias(sys_minherit,sys_madvise);
__strong_alias(sys_mlock,sys_madvise);
__strong_alias(sys_mlockall,sys_madvise);
__strong_alias(sys_munlock,sys_madvise);
__strong_alias(sys_munlockall,sys_madvise);
