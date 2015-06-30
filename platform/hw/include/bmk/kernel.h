#include <bmk/machine/md.h>

#ifndef _LOCORE

#include <bmk/types.h>

void bmk_halt(const char *) __attribute__((noreturn));

struct multiboot_info;
void bmk_multiboot(struct multiboot_info *);

void bmk_run(char *);

void bmk_cons_clear(void);
void bmk_cons_putc(int);
void bmk_cons_puts(const char *);

void bmk_cpu_init(void);
void bmk_cpu_block(bmk_time_t);
void bmk_cpu_nanohlt(void);
int bmk_cpu_intr_init(int);
void bmk_cpu_intr_ack(void);

bmk_time_t bmk_cpu_clock_now(void);
bmk_time_t bmk_cpu_clock_epochoffset(void);

void bmk_isr_clock(void);
void bmk_isr(int);
int bmk_intr_init(void);
int bmk_isr_init(int (*)(void *), void *, int);

void bmk_mainthread(void *);

#endif /* _LOCORE */

#include <bmk-core/errno.h>

#define BMK_MAXINTR	32

#define HZ 100
