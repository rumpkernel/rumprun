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

#include "netbsd_init.h"
#include "rumpconfig.h"

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
	int error;

	if (fd != -1 && prot != PROT_READ) {
		MMAP_PRINTF(("mmap: trying to r/w map a file. failing!\n"));
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
minherit(void *addr, size_t len, int inherit)
{
	/* nothing to inherit */
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
	/* XXX this duplicates _app_main / callmain cleanup */
	if (eval) {
		minios_printk("\n=== ERROR: _exit(%d) called ===\n", eval);
		/* XXX: work around the console being slow to attach */
		sleep(1);
	} else {
		minios_printk("\n=== _exit(%d) called ===\n", eval);
	}
	_rumprun_deconfig();
	_netbsd_fini();
	minios_stop_kernel();
	minios_do_halt(MINIOS_HALT_POWEROFF);
}
