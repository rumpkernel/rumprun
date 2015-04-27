#include <bmk/machine/md.h>

#ifndef _LOCORE

#include <bmk/types.h>

#define MEMSTART 0x100000
#define PAGE_SHIFT 12
#define PAGE_SIZE (1<<PAGE_SHIFT)
#define STACK_SIZE 0x2000

#define round_page(x) (((x) + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1))
#define trunc_page(x) ((x) & ~(PAGE_SIZE-1))

void *bmk_allocpg(size_t);

struct multiboot_info;
void bmk_init(void);
void bmk_halt(const char *) __attribute__((noreturn));
void bmk_main(struct multiboot_info *);

void bmk_cons_putc(int);

void bmk_cpu_init(void);
void bmk_cpu_nanohlt(void);
int bmk_cpu_intr_init(int);
void bmk_cpu_intr_ack(void);

bmk_time_t bmk_cpu_clock_now(void);

void bmk_isr_clock(void);
void bmk_isr(int);
int bmk_isr_init(void);
int bmk_isr_netinit(int (*)(void *), void *, int);

extern unsigned long bmk_membase, bmk_memsize;

#endif /* _LOCORE */

#include <bmk-core/errno.h>

#define BMK_MAXINTR	32
