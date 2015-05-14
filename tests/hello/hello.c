#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <rumprun/tester.h>

#if defined(__linux__) || !defined(__NetBSD__)
# error compiler wrapper fail
#endif

int
rumprun_test(int argc, char *argv[])
{
	char *world = getenv("WORLD");
	time_t now;

	if (world)
		printf ("Hello, %s!\n", world);
	else
		printf ("Hello, world!\n");

	now = time(NULL);
	printf("When you do not hear the beep the time will be exactly:\n%s",
	    ctime(&now));

	printf("Sleeping 1s...\n");
	sleep(1);

	now = time(NULL);
	printf("Goodbye, world, precisely at:\n%s", ctime(&now));

	return 0;
}
