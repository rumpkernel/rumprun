#ifndef _BMK_ARCH_I386_MD_H_
#define _BMK_ARCH_I386_MD_H_

#include <bmk/kernel.h>

#define ENTRY(x)        .text; .globl x; .type x,@function; x:
#define END(x)          .size x, . - x

#ifndef _LOCORE
struct region_descriptor;
void bmk_cpu_lidt(struct region_descriptor *);
void bmk_cpu_lgdt(struct region_descriptor *);

#include <bmk-core/platform.h>

static inline uint8_t
inb(uint16_t port)
{
        uint8_t rv;

        __asm__ __volatile__("inb %1, %0" : "=a"(rv) : "d"(port));

        return rv;
}

static inline uint32_t
inl(uint16_t port)
{
        uint32_t rv;

        __asm__ __volatile__("inl %1, %0" : "=a"(rv) : "d"(port));

        return rv;
}

static inline void
outb(uint16_t port, uint8_t value)
{

        __asm__ __volatile__("outb %0, %1" :: "a"(value), "d"(port));
}

static inline void
outl(uint16_t port, uint32_t value)
{

        __asm__ __volatile__("outl %0, %1" :: "a"(value), "d"(port));
}

extern int bmk_spldepth;

static inline void
splhigh(void)
{

	__asm__ __volatile__("cli");
	bmk_spldepth++;
}

static inline void
spl0(void)
{

	if (bmk_spldepth == 0)
		bmk_platform_halt("out of interrupt depth!");
	if (--bmk_spldepth == 0)
		__asm__ __volatile__("sti");
}

static inline void
hlt(void)
{

	__asm__ __volatile__("hlt");
}
#endif /* !_LOCORE */

#endif /* _BMK..._H_ */
