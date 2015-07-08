#ifndef _BMK_ARCH_X86_64_MD_H_
#define _BMK_ARCH_X86_64_MD_H_

#include <bmk/kernel.h>

#include <bmk-core/amd64/asm.h>

#define BMK_THREAD_STACK_PAGE_ORDER 3
#define BMK_THREAD_STACKSIZE ((1<<BMK_THREAD_STACK_PAGE_ORDER) * PAGE_SIZE)

#include <arch/x86/var.h>

#ifndef _LOCORE
#include <bmk-core/platform.h>

struct region_descriptor;
void bmk_amd64_lidt(struct region_descriptor *);
void bmk_amd64_ltr(unsigned long);

#include <arch/x86/reg.h>
#include <arch/x86/inline.h>

void bmk_cpu_boot(void *);
#endif /* !_LOCORE */

#endif /* _BMK..._H_ */
