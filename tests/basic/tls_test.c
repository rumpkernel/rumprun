/* Simple test for TLS support */
#include <sys/types.h>

#include <err.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

#include <rumprun/tester.h>

#define NTHREADS 10
#define LOOPCNT 1000
#define IINITIAL 12345678

static __thread unsigned long c;
static __thread unsigned long i = IINITIAL;

static void *
wrkthread(void *arg)
{
	unsigned long *ip = &i;

	printf("Thread %d starting\n", (int)(uintptr_t)arg);
	if (i != IINITIAL) {
		printf("initial i incorrect: %lu vs. %d\n", i, IINITIAL);
		return NULL;
	}
	i++;
	if (*ip != IINITIAL+1) {
		printf("initial *ip incorrect: %lu vs. %d\n", i, IINITIAL+1);
		return NULL;
	}
	i++;

	while (c < LOOPCNT) {
		c++;
		sched_yield();
	}
	return (void *)(uintptr_t)c;
}

int
rumprun_test(int argc, char *argv[])
{
	pthread_t threads[NTHREADS];
	int n, rc;

	printf("TLS test\n");

	for (n = 0; n < NTHREADS; n++) {
		rc = pthread_create(&threads[n], NULL,
		    wrkthread, (void *)(uintptr_t)n);
		if (rc != 0) {
			errx(1, "pthread_create[%d] failed: %d", n, rc);
		}
	}

	for (n = NTHREADS-1; n >= 0; n--) {
		void *ret;
		int rv;

		rc = pthread_join(threads[n], &ret);
		if (rc != 0) {
			errx(1, "pthread_join[%d] failed: %d", n, rc);
		}
		rv = (int)(uintptr_t)ret;
		if (rv != LOOPCNT)
			errx(1, "thread[%d]: expected %d, got %d",
			    n, LOOPCNT, rv);
	}
	return 0;
}
