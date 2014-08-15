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

int
rumpuser_getparam(const char *name, void *buf, size_t buflen)
{
	char *res = buf;

	if (bmk_strcmp(name, RUMPUSER_PARAM_NCPU) == 0
	    || bmk_strcmp(name, RUMPUSER_PARAM_HOSTNAME) == 0
	    || bmk_strcmp(name, "RUMP_VERBOSE") == 0) {
		res[0] = '1';
		res[1] = '\0';
		return 0;
	}
	return 1;
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
