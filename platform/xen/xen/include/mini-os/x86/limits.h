
#ifndef __ARCH_LIMITS_H__
#define __ARCH_LIMITS_H__

#define __PAGE_SHIFT      12

#ifdef __ASSEMBLY__
#define __PAGE_SIZE       (1 << __PAGE_SHIFT)
#else
/* XXX: must be ULL for correct integer promotion in pte calculations */
#define __PAGE_SIZE       (1ULL << __PAGE_SHIFT)
#endif

#define __STACK_SIZE_PAGE_ORDER  6
#define __STACK_SIZE             (__PAGE_SIZE * (1 << __STACK_SIZE_PAGE_ORDER))
          
#endif /* __ARCH_LIMITS_H__ */
