#include <sys/types.h>

#include <stdio.h>

/* Constructor test.  Checks that constructors run in the correct order */
int myvalue = 2;
static void __attribute__((constructor(2000),used))
ctor1(void)
{
	myvalue = myvalue * 2;
}

static void __attribute__((constructor(1000),used))
ctor2(void)
{
	myvalue = myvalue + 2;
}

static void __attribute__((constructor(1000),used))
ctor3(void)
{
	printf("I'm a constructor!\n");
}

static void __attribute__((destructor(1000),used))
dtor1(void)
{
	printf("I'm a destructor!\n");
}

int
main()
{

	if (myvalue != 8) {
		printf("ERROR running constructor test, myvalue=%d, "
		    "expected 8\n", myvalue);
		return 1;
	}
	return 0;
}
