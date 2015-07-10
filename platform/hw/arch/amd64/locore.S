/*-
 * Copyright (c) 2014, 2015 Antti Kantee.  All Rights Reserved.
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

#include <bmk/kernel.h>
#include <bmk/multiboot.h>

#define MYMULTIBOOT_FLAGS \
    (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_AOUT_KLUDGE)

.section .bootstrap, "ax"

.code32

.align 4
bootstrap:
.long MULTIBOOT_HEADER_MAGIC
.long MYMULTIBOOT_FLAGS
.long -(MULTIBOOT_HEADER_MAGIC+MYMULTIBOOT_FLAGS)
.long bootstrap
.long 0x100000
.long _edata
.long _ebss
.long _start

.space 4096
bootstack:

/*
 * Bootloader entry point.
 *
 * Bootstrap is slightly different from i386.  Multiboot puts us only
 * in 32bit mode, so it's our responsibility to install a page table
 * and switch to long mode.  Notably, we can't call C code until
 * we've switched to long mode.
 */
ENTRY(_start)
	cld
	movl $bootstack, %esp

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

	/* store multiboot structure address, process contents later in C */
	movl %ebx, bmk_cpu_multiboot_info

	lgdt (gdt64_ptr)
	pushl $0x0
	pushw $0x10
	pushl $1f
	lret

1:	movl $0x18, %eax
	movl %eax, %ds
	movl %eax, %es
	movl %eax, %ss

	xorl %eax, %eax
	movl %eax, %fs
	movl %eax, %gs

	/*
	 * x86_64 switch to long mode
	 */

	/* 1: enable pae and sse */
	movl %cr4, %eax
	orl $0x620, %eax
	movl %eax, %cr4

	/* 2: enable long mode */
	movl $0xc0000080, %ecx
	rdmsr
	movl $0x100, %eax
	wrmsr

	/* 3: load pml4 pointer */
	movl $bmk_cpu_pml4, %eax
	movl %eax, %cr3

	/* 4: enable paging */
	movl %cr0, %eax
	orl $0x80010001, %eax
	movl %eax, %cr0

	/* 5: poetically longjump to longmode */
	pushw $0x08
	pushl $_start64
	lret

	/* NOTREACHED */
	jmp haltme

nomultiboot:
	/*
	 * XXX: should print something, but can't call C code yet.
	 * write putc in asm or something else?
	 */
haltme:
	cli
	hlt
	jmp haltme
END(_start)

#include "pagetable.S"

/*
 * amd64 programmer's manual:
 *
 * "In long mode, segmentation is not used ... except for a few exceptions."
 *
 * Uuuyea, exceptions.
 */

.data
.align 64
.globl bmk_cpu_gdt64
bmk_cpu_gdt64:
	.quad 0x0000000000000000
	.quad 0x00af9b000000ffff	/* 64bit CS		*/
	.quad 0x00cf9b000000ffff	/* 32bit CS		*/
	.quad 0x00cf93000000ffff	/* DS			*/
	.quad 0x0000000000000000	/* TSS part 1 (via C)	*/
	.quad 0x0000000000000000	/* TSS part 2 (via C)	*/
gdt64_end:
.align 64

.type gdt64_ptr, @object
gdt64_ptr:
	.word gdt64_end-bmk_cpu_gdt64-1
	.quad bmk_cpu_gdt64

.code64

ENTRY(_start64)
	movq $bootstack, %rsp
	xorq %rbp, %rbp

	pushq $0x0
	pushq $0x0

	call bmk_cpu_boot
	hlt
END(_start64)

ENTRY(bmk_amd64_lidt)
	lidt (%rdi)
	ret
END(bmk_amd64_lidt)

ENTRY(bmk_amd64_ltr)
	ltr %di
	ret
END(bmk_amd64_ltr)
