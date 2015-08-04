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

#include <bmk-core/arm/asm.h>

ENTRY(bmk_cpu_sched_bouncer)
	pop {r1, r0}			/* pop function and argument */
	mov lr, pc
	bx r1
	bl bmk_sched_exit
END(bmk_cpu_sched_bouncer)

/*
 * r0 = previous thread
 * r1 = new thread
 */
ENTRY(bmk_cpu_sched_switch)
	push {r4-r12,r14}		/* save non-scratch regs except sp+pc */
	str sp, [r0, #0]		/* save sp */
	ldr sp, [r1, #0]		/* restore sp */
	adr r2, 1f			/* save pc */
	str r2, [r0, #4]
	ldr pc, [r1, #4]		/* restore pc */

1:
	pop {r4-r12,r14}		/* restore registers */
	bx lr
END(bmk_cpu_sched_switch)
