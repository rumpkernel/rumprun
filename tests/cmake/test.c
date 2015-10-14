#include "config.h"

#include <sys/types.h>

#include <stdio.h>

/*
 * What we're looking for with the next test is something that is likely
 * to be present on the host, but unlikely to be present on the target.
 * So we assume a Linux host.
 */

#ifdef HAVE_SYS_EPOLL_H
#error Should not have epoll.  In case NetBSD now has epoll, please update test.
#endif
#ifndef HAVE_SELECT
#error Could not find select.  Was ist los?
#endif

/* make sure we offer compat symbols that can be found by autoconf */
#ifndef HAVE_SIGACTION
#error sigaction not found.  libc problem.
#endif

int
main()
{

	printf("hello, test\n");
}
