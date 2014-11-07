#ifndef _MINIOS_KERNEL_H_
#define _MINIOS_KERNEL_H_

extern int app_main(start_info_t *);

extern void minios_do_exit(void) __attribute__((noreturn));
extern void stop_kernel(void);

#endif /* _MINIOS_KERNEL_H_ */
