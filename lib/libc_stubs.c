#include <mini-os/console.h>

#include <errno.h>

#define STUB(name)				\
  int name(void); int name(void) {		\
	static int done = 0;			\
	errno = ENOTSUP;			\
	if (done) return ENOTSUP; done = 1;	\
	minios_printk("STUB ``%s'' called\n", #name);	\
	return ENOTSUP;}

STUB(__sigaction14);
STUB(__getrusage50);

STUB(__wait450);
STUB(__fork);
STUB(__vfork14);
STUB(execve);
STUB(kill);
STUB(getpriority);
STUB(setpriority);
