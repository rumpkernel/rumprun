#ifndef _BMK_ARCH_ARM_MD_H_
#define _BMK_ARCH_ARM_MD_H_

#include <hw/kernel.h>

#define ENTRY(x)        .text; .globl x; .type x,%function; x:
#define END(x)          .size x, . - x

#define BMK_THREAD_STACK_PAGE_ORDER 1
#define BMK_THREAD_STACKSIZE ((1<<BMK_THREAD_STACK_PAGE_ORDER) * PAGE_SIZE)

#define PAGE_SHIFT 12
#define PAGE_SIZE (1<<PAGE_SHIFT)

#ifndef _LOCORE
#include <hw/machine/inline.h>

static inline void
splhigh(void)
{

	/* XXX TODO */
}

static inline void
spl0(void)
{

	/* XXX TODO */
}

static inline void
hlt(void)
{

	/* XXX TODO */
}

void	arm_boot(void);
void	arm_undefined(unsigned long *);
#endif

#endif /* _BMK..._H_ */
