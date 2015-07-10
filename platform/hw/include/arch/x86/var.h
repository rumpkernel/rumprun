#define PAGE_SHIFT 12
#define PAGE_SIZE (1<<PAGE_SHIFT)

#ifndef _LOCORE
void	bmk_x86_boot(void);

void	bmk_x86_initpic(void);
void	bmk_x86_initidt(void);
void	bmk_x86_initclocks(void);
void	bmk_x86_fillgate(int, void *, int);

/* trap "handlers" */
void bmk_x86_trap_0(void);
void bmk_x86_trap_2(void);
void bmk_x86_trap_3(void);
void bmk_x86_trap_4(void);
void bmk_x86_trap_5(void);
void bmk_x86_trap_6(void);
void bmk_x86_trap_7(void);
void bmk_x86_trap_8(void);
void bmk_x86_trap_10(void);
void bmk_x86_trap_11(void);
void bmk_x86_trap_12(void);
void bmk_x86_trap_13(void);
void bmk_x86_trap_14(void);
void bmk_x86_trap_17(void);

void bmk_x86_cpuid(uint32_t, uint32_t *, uint32_t *, uint32_t *, uint32_t *);
#endif
