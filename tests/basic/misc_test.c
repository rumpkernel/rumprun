#include <sys/times.h>

#include <pthread.h>
#include <stdio.h>

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

int
rumprun_test(int argc, char *argv[])
{
	int rv = 0;

	rv += test_times();
	rv += test_pthread_in_ctor();

	return rv;
}
