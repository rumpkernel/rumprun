#include <sys/types.h>

#include <sys/exec_elf.h>
#include <sys/exec.h>

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include "netbsd_init.h"

static char *the_env[1] = { NULL } ;
extern void *environ;
void _libc_init(void);
extern char *__progname;

/* XXX */
static struct ps_strings thestrings;
static AuxInfo myaux[2];
extern struct ps_strings *__ps_strings;
extern size_t pthread__stacksize;

typedef void (*initfini_fn)(void);
extern const initfini_fn __init_array_start[1];
extern const initfini_fn __init_array_end[1];
extern const initfini_fn __fini_array_start[1];
extern const initfini_fn __fini_array_end[1];

void *__dso_handle;

static void
runinit(void)
{
	const initfini_fn *fn;

	for (fn = __init_array_start; fn < __init_array_end; fn++)
		(*fn)();
}

static void
runfini(void)
{
	const initfini_fn *fn;

	for (fn = __fini_array_start; fn < __fini_array_end; fn++)
		(*fn)();
}

void
_netbsd_init(long stacksize)
{

	thestrings.ps_argvstr = (void *)((char *)&myaux - 2);
	__ps_strings = &thestrings;
	pthread__stacksize = 2*stacksize;

	rump_boot_setsigmodel(RUMP_SIGMODEL_IGNORE);
	rump_init();

	environ = the_env;
	rumprun_lwp_init();
	runinit();
	_libc_init();

	/* XXX: we should probably use csu, but this is quicker for now */
	__progname = "rumprun";

#ifdef RUMP_SYSPROXY
	rump_init_server("tcp://0:12345");
#endif

	/*
	 * give all threads a chance to run, and ensure that the main
	 * thread has gone through a context switch
	 */
	sched_yield();
}

void
_netbsd_fini(void)
{

	runfini();
	rump_sys_reboot(0, 0);
}
