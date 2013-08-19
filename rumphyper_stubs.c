#include <mini-os/console.h>

int ohcrap(void);
int ohcrap(void) {printk("rumphyper: unimplemented stub\n"); do_exit();}

int nothing(void); int nothing(void) {return 0;}

#define TIMETOPANIC(name) \
int name(void) __attribute__((alias("ohcrap")));

#define NOTHING(name) \
int name(void) __attribute__((alias("nothing")));

TIMETOPANIC(rumpuser_anonmmap);
TIMETOPANIC(rumpuser_unmap);

/* just so that we don't miss any */
TIMETOPANIC(rumpuser_dprintf);

/* signals AND sp not supported */
TIMETOPANIC(rumpuser_kill);

NOTHING(rumpuser_sp_init);
NOTHING(rumpuser_sp_fini);
TIMETOPANIC(rumpuser_sp_raise);
TIMETOPANIC(rumpuser_sp_copyin);
TIMETOPANIC(rumpuser_sp_copyout);
TIMETOPANIC(rumpuser_sp_copyinstr);
TIMETOPANIC(rumpuser_sp_copyoutstr);
TIMETOPANIC(rumpuser_sp_anonmmap);

NOTHING(rumpuser_dl_bootstrap);
NOTHING(rumpuser_dl_globalsym);

NOTHING(rumpuser_daemonize_begin);
NOTHING(rumpuser_daemonize_done);

TIMETOPANIC(rumpuser_iovread);
TIMETOPANIC(rumpuser_iovwrite);

NOTHING(rumpuser_close);
