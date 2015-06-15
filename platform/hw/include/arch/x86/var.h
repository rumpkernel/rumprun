#define ENTRY(x)        .text; .globl x; .type x,@function; x:
#define END(x)          .size x, . - x

#define PAGE_SHIFT 12
#define PAGE_SIZE (1<<PAGE_SHIFT)

#ifndef _LOCORE
void	bmk_cpu_initpic(void);
void	bmk_cpu_fillgate(int, void *, int);
#endif
