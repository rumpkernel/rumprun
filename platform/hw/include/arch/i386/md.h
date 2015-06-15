#ifndef _BMK_ARCH_I386_MD_H_
#define _BMK_ARCH_I386_MD_H_

#include <bmk/kernel.h>

#define BMK_THREAD_STACK_PAGE_ORDER 1
#define BMK_THREAD_STACKSIZE ((1<<BMK_THREAD_STACK_PAGE_ORDER) * PAGE_SIZE)

#include <arch/x86/var.h>

#ifndef _LOCORE
struct region_descriptor;
void bmk_cpu_lidt(struct region_descriptor *);
void bmk_cpu_lgdt(struct region_descriptor *);

#include <bmk-core/platform.h>

#include <arch/x86/reg.h>
#include <arch/x86/inline.h>

struct multiboot_info;
void bmk_cpu_boot(struct multiboot_info *);
#endif /* !_LOCORE */

#endif /* _BMK..._H_ */
