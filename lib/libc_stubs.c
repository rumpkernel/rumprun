#include <mini-os/console.h>

#include <errno.h>

#define STUB(name)				\
  int name(void); int name(void) {		\
	static int done = 0;			\
	errno = ENOTSUP;			\
	if (done) return ENOTSUP; done = 1;	\
	printk("STUB ``%s'' called\n", #name);	\
	return ENOTSUP;}

STUB(__sigaction14);
STUB(__sigprocmask14);
STUB(__getrusage50);

STUB(_lwp_kill);
STUB(_lwp_self);
STUB(__wait450);
STUB(__fork);
STUB(__vfork14);
STUB(execve);
