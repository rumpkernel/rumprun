#include <bmk/types.h>
#include <bmk/kernel.h>
#include <bmk/sched.h>
#include <bmk/memalloc.h>
#include <bmk/string.h>

#define LIBRUMPUSER
#include <rump/rumpuser.h>

#include "rumpuser_int.h"

void
rumpuser_putchar(int c)
{

	bmk_cons_putc(c);
}

/* reserve 1MB for bmk */
#define BMK_MEMRESERVE (1024*1024)

int
rumpuser_getparam(const char *name, void *buf, size_t buflen)
{
	int rv = 0;

	if (buflen <= 1)
		return 1;

	if (bmk_strcmp(name, RUMPUSER_PARAM_NCPU) == 0
	    || bmk_strcmp(name, "RUMP_VERBOSE") == 0) {
		bmk_strncpy(buf, "1", buflen-1);

	} else if (bmk_strcmp(name, RUMPUSER_PARAM_HOSTNAME) == 0) {
		bmk_strncpy(buf, "rump-baremetal", buflen-1);

	/* for memlimit, we have to implement int -> string ... */
	} else if (bmk_strcmp(name, "RUMP_MEMLIMIT") == 0) {
		unsigned long memsize;
		char tmp[64];
		char *res = buf;
		unsigned i, j;

		assert(bmk_memsize > BMK_MEMRESERVE);
		memsize = bmk_memsize - BMK_MEMRESERVE;

		for (i = 0; memsize > 0; i++) {
			assert(i < sizeof(tmp)-1);
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
		rv = 1;
	}

	return rv;
}

int
rumpuser_malloc(size_t howmuch, int alignment, void **memp)
{
	void *rv;

	if (howmuch == PAGE_SIZE)
		rv = bmk_allocpg(1);
	else
		rv = bmk_memalloc(howmuch, alignment);

	if (rv) {
		*memp = rv;
		return 0;
	}
	return ENOMEM;
}

void
rumpuser_free(void *mem, size_t len)
{

	if (len == PAGE_SIZE) {
		/* XXX: TODO */
	} else {
		bmk_memfree(mem);
	}
}

/* ok, this isn't really random, but let's make-believe */
int
rumpuser_getrandom(void *buf, size_t buflen, int flags, size_t *retp)
{
	static unsigned seed = 12345;
	unsigned *v = buf;

	*retp = buflen;
	while (buflen >= 4) {
		buflen -= 4;
		*v++ = seed;
		seed = (seed * 1103515245 + 12345) % (0x80000000L);
	}
	return 0;
}

int
rumpuser_clock_gettime(int enum_rumpclock, int64_t *sec, long *nsec)
{
	bmk_time_t now;

	now = bmk_clock_now();
	*sec  = now / (1000*1000*1000ULL);
	*nsec = now % (1000*1000*1000ULL);

	return 0;
}

int
rumpuser_clock_sleep(int enum_rumpclock, int64_t sec, long nsec)
{
	int nlocks;

	rumpkern_unsched(&nlocks, NULL);
	bmk_sched_nanosleep(sec * 1000*1000*1000ULL + nsec);
	rumpkern_sched(nlocks, NULL);

	return 0;
}

/*
 * currently, supports only printing fmt.  better than nothing ...
 */
void
rumpuser_dprintf(const char *fmt, ...)
{

	bmk_cons_puts(fmt);
}
