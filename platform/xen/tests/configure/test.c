#include "config.h"

#include <sys/types.h>

#include <stdio.h>

#ifdef HAVE_SYS_EPOLL_H
#error Should not have epoll.  In case NetBSD now has epoll, please update test.
#endif
#ifndef HAVE_SELECT
#error Could not find select.  Was ist los?
#endif

int
main()
{

	printf("hello, test\n");
}
