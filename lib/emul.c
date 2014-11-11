/* Copyright (c) 2013 Antti Kantee.  See COPYING */

/*
 * Emulate a bit of mmap.  Currently just MAP_ANON
 * and MAP_FILE+PROT_READ are supported.  For files, it's not true
 * mmap, but should cover a good deal of the cases anyway.
 */

/* for libc namespace */
#define mmap _mmap

#include <sys/cdefs.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <mini-os/os.h> /* for PAGE_SIZE */
#include <mini-os/kernel.h>

void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	void *v;
	ssize_t nn;
	int error;

	if (fd != -1 && prot != PROT_READ) {
		errno = ENOTSUP;
		return MAP_FAILED;
	}

	if ((error = posix_memalign(&v, len, PAGE_SIZE)) != 0) {
		errno = error;
		return MAP_FAILED;
	}

	if (flags & MAP_ANON)
		return v;

	if ((nn = pread(fd, v, len, off)) == -1) {
		error = errno;
		free(v);
		errno = error;
		return MAP_FAILED;
	}
	return v;
}
#undef mmap
__weak_alias(mmap,_mmap);

int
madvise(void *addr, size_t len, int adv)
{

	/* thanks for the advise, pal */
	return 0;
}

int
mprotect(void *addr, size_t len, int prot)
{
	/* no protection */
	return 0;
}

int
munmap(void *addr, size_t len)
{

	free(addr);
	return 0;
}

void __dead
_exit(int eval)
{

	minios_do_exit();
}
