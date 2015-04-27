#include <bmk/multiboot.h>
#include <bmk/kernel.h>

#define MYMULTIBOOT_FLAGS (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO)

.section .multiboot
.align 4
.long MULTIBOOT_HEADER_MAGIC
.long MYMULTIBOOT_FLAGS
.long -(MULTIBOOT_HEADER_MAGIC+MYMULTIBOOT_FLAGS)

.section bootstack
.set stack_size, 4096
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
	/* we don't have printf available yet, just output manually */
	mov $nomultimesg, %ebx
1:
	movsbl (%ebx), %eax
	test %al, %al
	je haltme
	movl %eax, 0(%esp)
	call bmk_cons_putc
	inc %ebx
	jmp 1b

haltme:
	cli
	hlt
	jmp haltme
END(_start32)

nomultimesg:
	.asciz "not multibooted, halting!"

ENTRY(bmk_cpu_lgdt)
	movl 4(%esp), %eax
	lgdt (%eax)
	movl $0x10, %eax
	movl %eax, %ds
	movl %eax, %es
	movl %eax, %ss
	movl %eax, %fs

	movl $0x18, %eax
	movl %eax, %gs

	popl %eax
	pushl $0x8
	pushl %eax
	lret
END(bmk_cpu_lgdt)

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

/*
 * The interrupt handlers don't do the normal accounting for cli-depth,
 * but it's hopefully a safe bet that since we're here, interrupts were
 * unmasked, so we can just unmask them before we return.
 */
ENTRY(bmk_cpu_isr_clock)
	cli
	call bmk_isr_clock
	movb $0x20, %al
	outb %al, $0x20
	sti
	iret
END(bmk_cpu_isr_clock)

/*
 * Macro to define interrupt stub to call C handler.
 * note: interrupt is acked on the PIC as part of bmk_isr
 */
#define INTRSTUB(intnum)						\
ENTRY(bmk_cpu_isr_##intnum)						\
	cli								;\
	pushl %eax							;\
	pushl %ebx							;\
	pushl %ecx							;\
	pushl %edx							;\
	pushl $intnum							;\
	call bmk_isr							;\
	addl $4, %esp							;\
	popl %edx							;\
	popl %ecx							;\
	popl %ebx							;\
	popl %eax							;\
	sti								;\
	iret								;\
END(bmk_cpu_isr_##intnum)

INTRSTUB(10)
INTRSTUB(11)
