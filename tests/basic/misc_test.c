#include <sys/param.h>
#include <sys/times.h>
#include <sys/mman.h>

#include <errno.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rumprun/tester.h>

static void
prfres(int res)
{

	if (res)
		printf("NOK\n");
	else
		printf("OK!\n");
}

/*
 * calling times() causes crash.  rumprun issue #4
 */
static int
test_times(void)
{
	struct tms tms;

	printf("checking that calling times() does not crash ... ");
	times(&tms);
	prfres(0);

	return 0;
}

/*
 * Check that pthread has been initialized when constructors are
 * called.  Issue reported by @liuw on irc
 */
static pthread_mutex_t ptm;
static void __attribute__((constructor))
pthmtxinit(void)
{

	pthread_mutex_init(&ptm, NULL);
}
static int
test_pthread_in_ctor(void)
{
	int rv;

	printf("checking that pthread_mutex_init() works from ctor ... ");
	rv = pthread_mutex_destroy(&ptm);
	prfres(rv);

	return rv;
}

static int
dommap(size_t len, int munmapperpage)
{
	void *v;

	v = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_ANON, -1, 0);
	if (v == MAP_FAILED)
		return errno;
	memset(v, 'a', len);
	if (!munmapperpage) {
		return munmap(v, len);
	} else {
		long pagesize = sysconf(_SC_PAGESIZE);
		const size_t roundedlen = roundup2(len, pagesize);
		uint8_t *p;
		int rv;

		for (p = v; p < (uint8_t *)v + roundedlen; p += pagesize) {
			if ((rv = munmap(p, pagesize)) != 0) {
				return rv;
			}
		}
	}

	return 0;
}

static int
test_mmap_anon(void)
{
	size_t lens[] = {4096, 6000, 8192, 12000, 12288, 65000};
	const long pagesize = sysconf(_SC_PAGESIZE);
	int rv;
	int i;

	printf("testing mmap(MAP_ANON) ...");
	for (i = 0; i < __arraycount(lens); i++) {
		if ((rv = dommap(lens[i], 0)) != 0)
			break;
		if ((rv = dommap(lens[i], 1)) != 0)
			break;
	}
	if (munmap((void *)0x1, pagesize) == 0) {
		rv = EINVAL;
		goto out;
	}
	if (munmap((void *)0x0, pagesize) == 0) {
		rv = EINVAL;
		goto out;
	}
 out:
	prfres(rv);

	return rv;
}

static int
test_etcpasswd(void)
{
	int rv;

	printf("testing that root is found in passwd ... ");
	rv = getpwnam("root") == NULL;
	prfres(rv);
	return rv;
}

int
rumprun_test(int argc, char *argv[])
{
	int rv = 0;

	rv += test_times();
	rv += test_pthread_in_ctor();
	rv += test_mmap_anon();
	rv += test_etcpasswd();

	return rv;
}
