/*-
 * Copyright (c) 2015 Antti Kantee.  All Rights Reserved.
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

#include <hw/kernel.h>

.space 4096
bootstack:

.space 4096
undefstack:

.section .start,"ax",%progbits

.globl _start
_start:
	/* set undefined stack */
	mrs r0, cpsr
	bic r0, r0, #0x1f
	orr r0, r0, #0x1b
	msr cpsr, r0
	ldr sp, =undefstack

	mrs r0, cpsr
	bic r0, r0, #0x1f
	orr r0, r0, #0x13
	msr cpsr, r0
	ldr sp, =bootstack
	bl arm_boot
	b haltme

haltme:
	b haltme

/*
 * "exception" vectors
 */
.globl vector_start, vector_end
vector_start:
	ldr pc, [pc, #24]
	ldr pc, [pc, #24]
	ldr pc, [pc, #24]
	ldr pc, [pc, #24]
	ldr pc, [pc, #24]
	nop
	ldr pc, [pc, #24]
	ldr pc, [pc, #24]

	.word	vector__start
	.word	vector_undefined
	.word	vector_softint
	.word	vector_prefetch_abort
	.word	vector_data_abort
	.word	0
	.word	vector_irq
	.word	vector_fiq
vector_end:

vector__start:
	b vector__start

vector_undefined:
	push {r0-r14}
	adds r0, sp, #(14*4)
	bl arm_undefined
	pop {r0-r14}
	movs pc, lr

/*
 * The rest of the exceptions just loop.  Attach a debugger
 * to find out where you are currently.
 */

vector_softint:
	b vector_softint

vector_prefetch_abort:
	b vector_prefetch_abort

vector_data_abort:
	b vector_data_abort

vector_irq:
	b vector_irq

vector_fiq:
	b vector_fiq
