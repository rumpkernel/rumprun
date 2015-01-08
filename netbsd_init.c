#include <sys/types.h>

#include <sys/exec_elf.h>
#include <sys/exec.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include <mini-os/os.h>
#include <mini-os/mm.h>

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

void
_netbsd_init(void)
{

	thestrings.ps_argvstr = (void *)((char *)&myaux - 2);
	__ps_strings = &thestrings;
	pthread__stacksize = 2*STACK_SIZE;

	rump_boot_setsigmodel(RUMP_SIGMODEL_IGNORE);
	rump_init();

	environ = the_env;
	_lwp_rumpxen_scheduler_init();
	_libc_init();

	/* XXX: we should probably use csu, but this is quicker for now */
	__progname = "rumpxenstack";

#ifdef RUMP_SYSPROXY
	rump_init_server("tcp://0:12345");
#endif
}

void
_netbsd_fini(void)
{

    rump_sys_reboot(0, 0);
}
