/*
 * This is a demonstration program to test pthreads on rumprun.
 * It's not very complete ...
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <rumprun/tester.h>

static pthread_mutex_t mtx;
static pthread_cond_t cv, cv2;

static int nthreads = 4;

static pthread_key_t thrnumkey;

static void
threxit(void *arg)
{

	pthread_mutex_lock(&mtx);
	if (--nthreads == 0) {
		printf("signalling\n");
		pthread_cond_signal(&cv2);
	}
	pthread_mutex_unlock(&mtx);

	if (pthread_getspecific(thrnumkey) != arg) {
		printf("ERROR: specificdata fail");
		abort();
	}

	printf("thread %p EXIT %d\n", arg, nthreads);
}

static void *
mythread(void *arg)
{

	printf("thread %p\n", arg);
	pthread_setspecific(thrnumkey, arg);

	pthread_mutex_lock(&mtx);
	printf("got lock %p\n", arg);
	sched_yield();
	pthread_mutex_unlock(&mtx);
	printf("unlocked lock %p\n", arg);
	sched_yield();

	threxit(arg);

	return NULL;
}

static int predicate;

static void *
waitthread(void *arg)
{

	printf("thread %p\n", arg);
	pthread_setspecific(thrnumkey, arg);
	pthread_mutex_lock(&mtx);
	while (!predicate) {
		printf("no good, need to wait %p\n", arg);
		pthread_cond_wait(&cv, &mtx);
	}
	pthread_mutex_unlock(&mtx);
	printf("condvar complete %p!\n", arg);

	threxit(arg);

	return NULL;
}

static void *
wakeupthread(void *arg)
{

	printf("thread %p\n", arg);
	pthread_setspecific(thrnumkey, arg);
	pthread_mutex_lock(&mtx);
	predicate = 1;
	printf("rise and shine %p!\n", arg);
	pthread_cond_signal(&cv);
	pthread_mutex_unlock(&mtx);

	threxit(arg);

	return NULL;
}

static void *
jointhread(void *arg)
{

	return (void *)37;
}

/* verify that a fd created in the main thread is accessible in another */
static void *
fdthread(void *arg)
{
	int fd = *(int *)arg;
	char buf[1];

	if (read(fd, buf, 1) != 0)
		err(1, "fdthread read");
	if (close(fd) != 0)
		err(1, "fdthread close");
	return (void *)0;
}

int
rumprun_test(int argc, char *argv[])
{
	struct timespec ts;
	pthread_t pt;
	void *rv;
	int nullfd;

	pthread_key_create(&thrnumkey, NULL);

	pthread_mutex_init(&mtx, NULL);
	pthread_cond_init(&cv, NULL);
	pthread_cond_init(&cv2, NULL);

	printf("testing pthread_join\n");
	if (pthread_create(&pt, NULL, jointhread, NULL) != 0)
		errx(1, "pthread jointhread create");
	if (pthread_join(pt, &rv) != 0)
		errx(1, "pthread_join()");
	if (rv != (void *)37)
		errx(1, "joiner returned incorrect value");
	printf("success\n");

	if (pthread_create(&pt, NULL, mythread, (void *)0x01) != 0)
		errx(1, "pthread_create()");

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_nsec += 100*1000*1000;
	pthread_mutex_lock(&mtx);
	if (pthread_cond_timedwait(&cv2, &mtx, &ts) != ETIMEDOUT) {
		printf("cond_timedwait fail\n");
		abort();
	}
	pthread_mutex_unlock(&mtx);

	if (pthread_create(&pt, NULL, mythread, (void *)0x02) != 0)
		errx(1, "pthread_create()");
	if (pthread_create(&pt, NULL, waitthread, (void *)0x03) != 0)
		errx(1, "pthread_create()");
	if (pthread_create(&pt, NULL, wakeupthread, (void *)0x04) != 0)
		errx(1, "pthread_create()");

	pthread_mutex_lock(&mtx);
	/* get time after locking => ensure loop runs before threads finish */
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec++;
	while (nthreads) {
		int rv;

		printf("mainthread condwaiting\n");
		if ((rv = pthread_cond_timedwait(&cv2, &mtx, &ts)) != 0) {
			printf("drain condwait fail %d %d\n", rv, nthreads);
		}
	}
	pthread_mutex_unlock(&mtx);

	if ((nullfd = open("/dev/null", O_RDONLY)) < 0)
		err(1, "open(/dev/null)");
	if (pthread_create(&pt, NULL, fdthread, (void *)&nullfd) != 0)
		errx(1, "pthread_create()");
	if (pthread_join(pt, &rv) != 0)
		errx(1, "pthread_join()");

	printf("main thread done\n");

	return 0;
}
