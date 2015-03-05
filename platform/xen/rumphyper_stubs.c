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

NOTHING(rumpuser_dl_bootstrap);

NOTSUP(rumpuser_daemonize_begin);
NOTSUP(rumpuser_daemonize_done);

NOTSUP(rumpuser_iovread);
NOTSUP(rumpuser_iovwrite);
