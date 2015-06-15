#define ENTRY(x)        .text; .globl x; .type x,@function; x:
#define END(x)          .size x, . - x

#define PAGE_SHIFT 12
#define PAGE_SIZE (1<<PAGE_SHIFT)

#ifndef _LOCORE
void	bmk_x86_initpic(void);
void	bmk_x86_inittimer(void);
void	bmk_x86_fillgate(int, void *, int);
#endif
