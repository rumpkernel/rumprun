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

#include <hw/multiboot.h>
#include <hw/kernel.h>

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
	movq $itsatrap, %rdi						;\
	call cons_puts							;\
	movq $trapstr##num, %rdi					;\
	call cons_puts							;\
	movq $'\n', %rdi						;\
	call cons_putc							;\
	iretq								;\
END(x86_trap_##num)

SYSTRAP(2, "#NMI")
SYSTRAP(3, "#BP")
SYSTRAP(4, "#OF")
SYSTRAP(5, "#BR")
SYSTRAP(7, "#NM")
SYSTRAP(8, "#DF")
SYSTRAP(10, "#TS")
SYSTRAP(11, "#NP")
SYSTRAP(12, "#SS")
SYSTRAP(17, "#AC")

/*
 * Fatal traps.  Well the previous ones might be fatal too, but let's
 * call these especially fatal.
 */
#define FATTRAP(num, reason)						 \
fattrapstr##num:							;\
	.asciz reason							;\
ENTRY(x86_trap_##num)							 \
	movq $fattrapstr##num, %rdi					;\
	movq %cr2, %rdx							;\
	movq $##num, %rcx						;\
	cmpq $13, %rcx							;\
	je 1f								;\
	cmpq $14, %rcx							;\
	jne 2f								;\
1:									;\
	addq $8, %rsp							;\
2:									;\
	movq 0(%rsp), %rsi						;\
	call cpu_fattrap						;\
	hlt								;\
END(x86_trap_##num)

FATTRAP(0, "divide-by-zero")
FATTRAP(6, "invalid opcode")
FATTRAP(13, "general protection")
FATTRAP(14, "page fault")

/*
 * we just ignore most interrupts and traps with this
 * convenient inservice routine
 */
ENTRY(cpu_insr)
	cli
	pushq %rax
	movb $0x20, %al
	outb %al, $0x20
	popq %rax
	sti
	iretq
END(cpu_insr)

/*
 * The interrupt handlers don't do the normal accounting for cli-depth,
 * but it's hopefully a safe bet that since we're here, interrupts were
 * unmasked, so we can just unmask them before we return.
 */
ENTRY(cpu_isr_clock)
	cli
	pushq %rax
	movb $0x20, %al
	outb %al, $0x20
	popq %rax
	sti
	iretq
END(cpu_isr_clock)

/*
 * Macro to define interrupt stub to call C handler.
 * note: interrupt is acked on the PIC as part of isr
 */
#define INTRSTUB(intnum)						\
ENTRY(x86_isr_##intnum)							\
	cli								;\
	pushq %rax							;\
	pushq %rbx							;\
	pushq %rcx							;\
	pushq %rdx							;\
	pushq %rdi							;\
	pushq %rsi							;\
	movq $(1<<intnum), %rdi						;\
	call isr							;\
	popq %rsi							;\
	popq %rdi							;\
	popq %rdx							;\
	popq %rcx							;\
	popq %rbx							;\
	popq %rax							;\
	sti								;\
	iretq								;\
END(x86_isr_##intnum)

INTRSTUB(9)
INTRSTUB(10)
INTRSTUB(11)
INTRSTUB(14)
INTRSTUB(15)
