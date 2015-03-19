#include <bmk/machine/md.h>

#ifndef _LOCORE

#include <bmk/types.h>

#define MEMSTART 0x100000
#define PAGE_SIZE 0x1000
#define STACK_SIZE 0x2000

#define round_page(x) (((x) + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1))
#define trunc_page(x) ((x) & ~(PAGE_SIZE-1))

#define assert(x) \
  do { \
    if (!(x)) { \
        bmk_cons_puts("assert \"" #x "\" FAILED\n"); for (;;); \
    } \
  } while (0)

void *bmk_allocpg(size_t);

#define panic(x) do { bmk_cons_puts(x "\n"); for (;;); } while (0)

/* eh eh eh.  NOTE: callable only after rump_init() has run! */
#define bmk_printf(x, ...) \
    do { \
	void rump_schedule(void); void rump_unschedule(void); \
	int rumpns_printf(const char *, ...); \
	rump_schedule(); rumpns_printf(x, __VA_ARGS__); rump_unschedule(); \
    } while (0);

struct multiboot_info;
void bmk_init(void);
void bmk_halt(void);
void bmk_main(struct multiboot_info *);

void bmk_cons_puts(const char *);
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

#include <bmk-common/errno.h>

#define BMK_MAXINTR	32
