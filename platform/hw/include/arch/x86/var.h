#ifndef _LOCORE
struct multiboot_info;
void	x86_boot(struct multiboot_info *);

void	x86_initpic(void);
void	x86_initidt(void);
void	x86_initclocks(void);
void	x86_fillgate(int, void *, int);

/* trap "handlers" */
void x86_trap_0(void);
void x86_trap_2(void);
void x86_trap_3(void);
void x86_trap_4(void);
void x86_trap_5(void);
void x86_trap_6(void);
void x86_trap_7(void);
void x86_trap_8(void);
void x86_trap_10(void);
void x86_trap_11(void);
void x86_trap_12(void);
void x86_trap_13(void);
void x86_trap_14(void);
void x86_trap_17(void);

void x86_cpuid(uint32_t, uint32_t *, uint32_t *, uint32_t *, uint32_t *);

extern uint8_t pic1mask, pic2mask;
#endif
