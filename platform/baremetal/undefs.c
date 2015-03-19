#include <bmk/kernel.h>

/*
 * Stubs for unused rump kernel hypercalls
 */

#define NOTHING(name) \
    int name(void); int name(void) \
    {bmk_cons_puts("unimplemented: " #name "\n"); for (;;);}

#define REALNOTHING(name) \
    int name(void); int name(void) {return 1;}

NOTHING(rumpuser_anonmmap)

REALNOTHING(rumpuser_dl_bootstrap)

NOTHING(rumpuser_daemonize_begin)
NOTHING(rumpuser_daemonize_done)
NOTHING(rumpuser_exit)
NOTHING(rumpuser_kill)

NOTHING(rumpuser_open)
NOTHING(rumpuser_close)
REALNOTHING(rumpuser_getfileinfo)
NOTHING(rumpuser_bio)
NOTHING(rumpuser_iovread)
NOTHING(rumpuser_iovwrite)

NOTHING(rumpuser_unmap)

/* libc */

REALNOTHING(__sigaction14);
NOTHING(__getrusage50);
//REALNOTHING(__sigprocmask14);
REALNOTHING(sigqueueinfo);
//REALNOTHING(rasctl);
NOTHING(_lwp_kill);
//NOTHING(_lwp_self);

//NOTHING(__libc_static_tls_setup);

NOTHING(__fork);
NOTHING(__vfork14);
NOTHING(kill);
NOTHING(getpriority);
NOTHING(setpriority);

int execve(const char *, char *const[], char *const[]);
int
execve(const char *file, char *const argv[], char *const envp[])
{

	bmk_cons_puts("execve not implemented\n");
	return -1;
}

NOTHING(_sys_mq_send);
NOTHING(_sys_mq_receive);
NOTHING(_sys___mq_timedsend50);
NOTHING(_sys___mq_timedreceive50);
NOTHING(_sys_msgrcv);
NOTHING(_sys_msgsnd);
NOTHING(_sys___msync13);
NOTHING(_sys___wait450);
NOTHING(_sys___sigsuspend14);
