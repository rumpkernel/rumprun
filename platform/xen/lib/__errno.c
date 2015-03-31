/* Copyright (c) 2013 Antti Kantee.  See COPYING */

#include <mini-os/sched.h>

#include <errno.h>
#undef __errno

int *
__errno(void)
{

	return minios_sched_geterrno();
}
