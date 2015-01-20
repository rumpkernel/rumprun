#include <sys/types.h>

#include <sys/exec_elf.h>
#include <sys/exec.h>

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include <bmk/sched.h>

static char *the_env[1] = { NULL } ;
extern void *environ;
void _libc_init(void);
extern char *__progname;

/* XXX */
static struct ps_strings thestrings;
static AuxInfo myaux[2];
extern struct ps_strings *__ps_strings;
extern size_t pthread__stacksize;

#include "netbsd_init.h"

#include <bmk/sched.h>

void
_netbsd_init(void)
{

	thestrings.ps_argvstr = (void *)((char *)&myaux - 2);
	__ps_strings = &thestrings;
	pthread__stacksize = 2*BMK_THREAD_STACKSIZE;

	environ = the_env;
	bmk_lwp_init();
	_libc_init();

	/* XXX: we should probably use csu, but this is quicker for now */
	__progname = "baremetal";

	/*
	 * give all threads a chance to run, and ensure that the main
	 * thread has gone through a context switch
	 */
	sched_yield();
}

#if 0
void
_netbsd_fini(void)
{

    rump_sys_reboot(0, 0);
}
#endif
