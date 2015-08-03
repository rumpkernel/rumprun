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

#include <hw/kernel.h>

void
x86_initpic(void)
{

	/*
	 * init pic1: cycle is write to cmd followed by 3 writes to data
	 */
	outb(PIC1_CMD, ICW1_INIT | ICW1_IC4);
	outb(PIC1_DATA, 32);	/* interrupts start from 32 in IDT */
	outb(PIC1_DATA, 1<<2);	/* slave is at IRQ2 */
	outb(PIC1_DATA, ICW4_8086);
	outb(PIC1_DATA, 0xff & ~(1<<2));	/* unmask slave IRQ */

	/* do the slave PIC */
	outb(PIC2_CMD, ICW1_INIT | ICW1_IC4);
	outb(PIC2_DATA, 32+8);	/* interrupts start from 40 in IDT */
	outb(PIC2_DATA, 2);	/* interrupt at irq 2 */
	outb(PIC2_DATA, ICW4_8086);
	outb(PIC2_DATA, 0xff);	/* all masked */
}

/* interrupt-not-service-routine */
void cpu_insr(void);

void
x86_initidt(void)
{
	int i;

	for (i = 0; i < 48; i++) {
		x86_fillgate(i, cpu_insr, 0);
	}

#define FILLGATE(n) x86_fillgate(n, x86_trap_##n, 0)
	FILLGATE(0);
	FILLGATE(2);
	FILLGATE(3);
	FILLGATE(4);
	FILLGATE(5);
	FILLGATE(6);
	FILLGATE(7);
	FILLGATE(8);
	FILLGATE(10);
	FILLGATE(11);
	FILLGATE(12);
	FILLGATE(13);
	FILLGATE(14);
	FILLGATE(17);
#undef FILLGATE
	x86_fillgate(2, x86_trap_2, 2);
	x86_fillgate(8, x86_trap_8, 3);
}

void
x86_cpuid(uint32_t level, uint32_t *eax_out, uint32_t *ebx_out,
		uint32_t *ecx_out, uint32_t *edx_out)
{
	uint32_t eax_, ebx_, ecx_, edx_;

	/*
	 * Verify if the requested CPUID level is supported. If not, just
	 * zero everything and return, hoping the caller knows what to do.
	 * This is better than the documented operation for invalid values of
	 * level, which is to behave as if CPUID had been called with the
	 * maximum supported level.
	 */
	eax_ = (level < 0x80000000) ? 0 : 0x80000000;
	__asm__(
		"cpuid"
		: "=a" (eax_), "=b" (ebx_), "=c" (ecx_), "=d" (edx_)
		: "0" (eax_)
	);
	if (eax_ < level) {
		*eax_out = *ebx_out = *ecx_out = *edx_out = 0;
		return;
	}

	/*
	 * Call requested CPUID level.
	 */
	__asm__(
		"cpuid"
		: "=a" (eax_), "=b" (ebx_), "=c" (ecx_), "=d" (edx_)
		: "0" (level)
	);
	*eax_out = eax_;
	*ebx_out = ebx_;
	*ecx_out = ecx_;
	*edx_out = edx_;
}
