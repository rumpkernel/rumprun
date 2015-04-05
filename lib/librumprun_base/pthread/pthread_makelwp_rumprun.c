#include "pthread_makelwp.h"

#include <bmk-base/rumprun_makelwp.h>

int
pthread__makelwp(void (*start)(void *), void *arg, void *private,
	void *stack_base, size_t stack_size, unsigned long flags, lwpid_t *lid)
{

	return rumprun_makelwp(start, arg, private,
	    stack_base, stack_size, flags, lid);
}
