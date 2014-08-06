#include <errno.h>
#undef __errno

#include <bmk/sched.h>

int *
__errno(void)
{

	return bmk_sched_geterrno();
}
