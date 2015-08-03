#ifndef _BMK_ARCH_ARM_MD_H_
#define _BMK_ARCH_ARM_MD_H_

#include <hw/kernel.h>

#define ENTRY(x)        .text; .globl x; .type x,%function; x:
#define END(x)          .size x, . - x

#ifndef _LOCORE
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
#endif

#endif /* _BMK..._H_ */
