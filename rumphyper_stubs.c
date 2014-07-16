#include <errno.h>

int stub_nothing(void); int stub_nothing(void) {return 0;}

int stub_enotsup(void); int stub_enotsup(void) {return ENOTSUP;}

#define NOTHING(name) \
int name(void) __attribute__((alias("stub_nothing")));

#define NOTSUP(name) \
int name(void) __attribute__((alias("stub_enotsup")));

NOTSUP(rumpuser_anonmmap);
NOTSUP(rumpuser_unmap);

NOTSUP(rumpuser_kill);

NOTSUP(rumpuser_sp_init);
NOTHING(rumpuser_sp_fini);
NOTSUP(rumpuser_sp_raise);
NOTSUP(rumpuser_sp_copyin);
NOTSUP(rumpuser_sp_copyout);
NOTSUP(rumpuser_sp_copyinstr);
NOTSUP(rumpuser_sp_copyoutstr);
NOTSUP(rumpuser_sp_anonmmap);

NOTHING(rumpuser_dl_bootstrap);
NOTHING(rumpuser_dl_globalsym);

NOTSUP(rumpuser_daemonize_begin);
NOTSUP(rumpuser_daemonize_done);

NOTSUP(rumpuser_iovread);
NOTSUP(rumpuser_iovwrite);
