#ifndef _MINIOS_KERNEL_H_
#define _MINIOS_KERNEL_H_

extern int app_main(start_info_t *);

extern void minios_do_exit(void) __attribute__((noreturn));
extern void minios_stop_kernel(void);

/* Values should mirror SHUTDOWN_* in xen/sched.h */
#define MINIOS_HALT_POWEROFF 0
#define MINIOS_HALT_CRASH 3
extern void minios_do_halt(int reason) __attribute__((noreturn));

#endif /* _MINIOS_KERNEL_H_ */
