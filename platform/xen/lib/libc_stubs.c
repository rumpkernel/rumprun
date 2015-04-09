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
STUB(__sigaction_sigtramp);
STUB(sigaction);
STUB(sigprocmask);
STUB(__getrusage50);

STUB(__fork);
STUB(__vfork14);
STUB(execve);
STUB(kill);
STUB(getpriority);
STUB(setpriority);
STUB(posix_spawn);

STUB(mlockall);

/* for pthread_cancelstub */
STUB(_sys_mq_send);
STUB(_sys_mq_receive);
STUB(_sys___mq_timedsend50);
STUB(_sys___mq_timedreceive50);
STUB(_sys_msgrcv);
STUB(_sys_msgsnd);
STUB(_sys___msync13);
STUB(_sys___wait450);
STUB(_sys___sigsuspend14);
