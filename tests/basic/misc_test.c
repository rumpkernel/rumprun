#include <sys/times.h>

#include <stdio.h>

#include <rumprun/tester.h>

/*
 * calling times() causes crash.  rumprun issue #4
 */
static int
test_times(void)
{
	struct tms tms;
	
	printf("checking that calling times() does not crash ...\n");
	times(&tms);
	printf("OK!\n");

	return 0;
}

int
rumprun_test(int argc, char *argv[])
{

	test_times();

	return 0;
}
