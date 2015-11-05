/*-
 * Copyright (c) 2014 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <hw/multiboot.h>
#include <hw/kernel.h>

#define MYMULTIBOOT_FLAGS (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO)

.section .multiboot
.align 4
.long MULTIBOOT_HEADER_MAGIC
.long MYMULTIBOOT_FLAGS
.long -(MULTIBOOT_HEADER_MAGIC+MYMULTIBOOT_FLAGS)

.space 4096
bootstack:

ENTRY(_start)
	cld
	movl $bootstack, %esp

	/* save multiboot info pointer */
	pushl %ebx

	/* save BIOS data area values */
	movw BIOS_COM1_BASE, %bx
	movw %bx, bios_com1_base
	movw BIOS_CRTC_BASE, %bx
	movw %bx, bios_crtc_base

	/* clear console */
	pushl %eax
	movw $0x20, %ax
	movl $(CONS_ADDRESS), %edi
	movl $(CONS_WIDTH*CONS_HEIGHT), %ecx
	rep stosw
	popl %eax

	/* only multiboot is supported for now */
	cmpl $MULTIBOOT_BOOTLOADER_MAGIC, %eax
	jne nomultiboot

	call x86_boot

	jmp haltme

nomultiboot:
	/* we don't have printf available yet, just output manually */
	mov $nomultimesg, %ebx
	mov $(CONS_ADDRESS), %ecx
1:
	movsbl (%ebx), %eax
	test %al, %al
	je haltme
	orl $0x500, %eax
	movl %eax, (%ecx)
	inc %ebx
	addl $2, %ecx
	jmp 1b

haltme:
	cli
	hlt
	jmp haltme
END(_start)

nomultimesg:
	.asciz "not multibooted, halting!"

ENTRY(cpu_lgdt)
	movl 4(%esp), %eax
	lgdt (%eax)
	movl $0x10, %eax
	movl %eax, %ds
	movl %eax, %es
	movl %eax, %fs
	movl %eax, %ss
	movl %eax, %gs

	popl %eax
	pushl $0x8
	pushl %eax
	lret
END(cpu_lgdt)

ENTRY(cpu_lidt)
	movl 4(%esp), %eax
	lidt (%eax)
	ret
END(cpu_lidt)

/*
 * Default trap handlers, which just print info on what type of trap
 * it was.  Generally speaking, we don't except trouble here.
 */
itsatrap:
	.asciz "trap: "
#define SYSTRAP(num, reason)						 \
trapstr##num:								;\
	.asciz reason							;\
ENTRY(x86_trap_##num)							 \
	pushl $itsatrap							;\
	call cons_puts							;\
	pushl $trapstr##num						;\
	call cons_puts							;\
	pushl $'\n'							;\
	call cons_putc							;\
	addl $12, %esp							;\
	iret								;\
END(x86_trap_##num)

SYSTRAP(0, "#DE")
SYSTRAP(2, "#NMI")
SYSTRAP(3, "#BP")
SYSTRAP(4, "#OF")
SYSTRAP(5, "#BR")
SYSTRAP(6, "#UD")
SYSTRAP(7, "#NM")
SYSTRAP(8, "#DF")
SYSTRAP(10, "#TS")
SYSTRAP(11, "#NP")
SYSTRAP(12, "#SS")
SYSTRAP(13, "#GP")
SYSTRAP(14, "#PF")
SYSTRAP(17, "#AC")

/*
 * we just ignore most interrupts and traps with this
 * convenient inservice routine
 */
ENTRY(cpu_insr)
	cli
	pushl %eax
	movb $0x20, %al
	outb %al, $0x20
	popl %eax
	sti
	iret
END(cpu_insr)

/*
 * The interrupt handlers don't do the normal accounting for cli-depth,
 * but it's hopefully a safe bet that since we're here, interrupts were
 * unmasked, so we can just unmask them before we return.
 */
ENTRY(cpu_isr_clock)
	cli
	pushl %eax
	movb $0x20, %al
	outb %al, $0x20
	popl %eax
	sti
	iret
END(cpu_isr_clock)

/*
 * Macro to define interrupt stub to call C handler.
 * note: interrupt is acked on the PIC as part of isr
 */
#define INTRSTUB(intnum)						\
ENTRY(x86_isr_##intnum)							\
	cli								;\
	pushl %eax							;\
	pushl %ebx							;\
	pushl %ecx							;\
	pushl %edx							;\
	pushl $(1<<intnum)						;\
	call isr							;\
	addl $4, %esp							;\
	popl %edx							;\
	popl %ecx							;\
	popl %ebx							;\
	popl %eax							;\
	sti								;\
	iret								;\
END(x86_isr_##intnum)

INTRSTUB(9)
INTRSTUB(10)
INTRSTUB(11)
INTRSTUB(14)
INTRSTUB(15)
