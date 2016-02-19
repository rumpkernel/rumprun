#include <hw/machine/md.h>

#ifndef _LOCORE

#include <hw/types.h>

struct multiboot_info;
void multiboot(struct multiboot_info *);

void cons_init(void);
void cons_putc(int);
void cons_puts(const char *);

void cpu_init(void);
void cpu_block(bmk_time_t);
int cpu_intr_init(int);
void cpu_intr_ack(unsigned);

bmk_time_t cpu_clock_now(void);
bmk_time_t cpu_clock_epochoffset(void);

void isr(int);
void intr_init(void);
void bmk_isr_rumpkernel(int (*)(void *), void *, int, int);

#define BMK_INTR_ROUTED 0x01

#define BMK_MULTIBOOT_CMDLINE_SIZE 4096
extern char multiboot_cmdline[];

#endif /* _LOCORE */

#include <bmk-core/errno.h>

#define BMK_MAXINTR	32

#define HZ 100
