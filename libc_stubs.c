#include <mini-os/console.h>

#define STUB(name)				\
  int name(void); int name(void) {		\
	static int done = 0;			\
	if (done) return 1; done = 1;		\
	printk("STUB ``%s'' called\n", #name);	\
	return 2;}

#define STUBNULL(name)				\
  void *name(void); void *name(void) {		\
	static int done = 0;			\
	if (done) return NULL; done = 1;	\
	printk("STUB ``%s'' called\n", #name);	\
	return NULL;}

STUB(__clock_gettime50);
STUB(__nanosleep50);
STUB(__setitimer50);
STUB(__sigaction14);
STUB(__sigprocmask14);

STUB(_exit);
STUB(_lwp_kill);
STUB(_lwp_self);
STUB(_mmap);
STUB(munmap);
STUB(__wait450);
STUB(__fork);

STUBNULL(_citrus_LC_CTYPE_setlocale);
STUBNULL(_citrus_LC_MESSAGES_setlocale);
STUBNULL(_citrus_LC_MONETARY_setlocale);
STUBNULL(_citrus_LC_NUMERIC_setlocale);
STUBNULL(_citrus_LC_TIME_setlocale);
STUBNULL(_citrus_ctype_default);
STUBNULL(_citrus_lookup_simple);
