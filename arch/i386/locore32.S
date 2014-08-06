#include <bmk/multiboot.h>
#include <bmk/kernel.h>

#define MYMULTIBOOT_FLAGS (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO)

.section .multiboot
.align 4
.long MULTIBOOT_HEADER_MAGIC
.long MYMULTIBOOT_FLAGS
.long -(MULTIBOOT_HEADER_MAGIC+MYMULTIBOOT_FLAGS)

.section bootstack
.set stack_size, 8192
.globl _bootstack
.comm _bootstack, stack_size

ENTRY(_start32)
	cld
	movl $(_bootstack + stack_size), %esp

	pushl %ebx
	pushl %eax
	call bmk_init
	popl %eax

	/* only multiboot is supported for now */
	cmpl $MULTIBOOT_BOOTLOADER_MAGIC, %eax
	jne nomultiboot

	call bmk_main
	jmp haltme

nomultiboot:
	pushl $nomultimesg
	call bmk_cons_puts
	hlt

haltme:
	cli
	hlt
	jmp haltme
END(_start32)

nomultimesg:
	.asciz "not multibooted, halting!"

ENTRY(bmk_cpu_lidt)
	movl 4(%esp), %eax
	lidt (%eax)
	ret
END(bmk_cpu_lidt)

/*
 * we just ignore most interrupts and traps with this
 * convenient inservice routine
 */
ENTRY(bmk_cpu_insr)
	iret
END(bmk_cpu_insr)

ENTRY(bmk_cpu_isr_clock)
	cli
	call bmk_isr_clock
	movb $0x20, %al
	outb %al, $0x20
	sti
	iret
END(bmk_cpu_isr_clock)

/*
 * note: interrupt is acked on the PIC as part of bmk_isr_net
 */
ENTRY(bmk_cpu_isr_net)
	cli
	call bmk_isr_net
	sti
	iret
END(bmk_cpu_isr_net)


/*
 * From Xen MiniOS arch/x86/x86_32.S.  No copyright there, so I
 * assume the one in ./cpu_sched.c applies.
 */

ENTRY(bmk_cpu_sched_bouncer)
	popl %eax
	popl %ebx
	pushl $0
	xorl %ebp,%ebp
	pushl %eax
	call *%ebx
	/* call exit_thread  */		/* hope no threads exit ;) */
END(bmk_cpu_sched_bouncer)

ENTRY(bmk_cpu_sched_switch)
	movl 4(%esp), %ecx		/* prev */
	movl 8(%esp), %edx		/* next */
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	movl %esp, (%ecx)		/* save ESP */
	movl (%edx), %esp		/* restore ESP */
	movl $1f, 4(%ecx)		/* save EIP */
	pushl 4(%edx)			/* restore EIP */
	ret
1:
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret
END(bmk_cpu_sched_switch)
