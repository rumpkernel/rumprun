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
NOTHING(rumpuser_dprintf)
NOTHING(rumpuser_exit)
NOTHING(rumpuser_kill)

NOTHING(rumpuser_open)
NOTHING(rumpuser_close)
REALNOTHING(rumpuser_getfileinfo)
NOTHING(rumpuser_bio)
NOTHING(rumpuser_iovread)
NOTHING(rumpuser_iovwrite)

NOTHING(rumpuser_sp_anonmmap)
NOTHING(rumpuser_sp_copyin)
NOTHING(rumpuser_sp_copyinstr)
NOTHING(rumpuser_sp_copyout)
NOTHING(rumpuser_sp_copyoutstr)
NOTHING(rumpuser_sp_fini)
NOTHING(rumpuser_sp_init)
NOTHING(rumpuser_sp_raise)
NOTHING(rumpuser_unmap)
