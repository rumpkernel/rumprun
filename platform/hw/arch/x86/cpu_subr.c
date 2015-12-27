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
#include <arch/x86/var.h>

void x86_isr_9(void);
void x86_isr_10(void);
void x86_isr_11(void);
void x86_isr_14(void);
void x86_isr_15(void);

uint8_t pic1mask, pic2mask;

int
cpu_intr_init(int intr)
{

	if (intr > 15)
		return BMK_EGENERIC;

#define FILLGATE(n) case n: x86_fillgate(32+n, x86_isr_##n, 0); break
	switch (intr) {
		FILLGATE(9);
		FILLGATE(10);
		FILLGATE(11);
		FILLGATE(14);
		FILLGATE(15);
	default:
		return BMK_EGENERIC;
	}
#undef FILLGATE

	/* unmask interrupt in PIC */
	if (intr < 8) {
		pic1mask &= ~(1<<intr);
		outb(PIC1_DATA, pic1mask);
	} else {
		pic2mask &= ~(1<<(intr-8));
		outb(PIC2_DATA, pic2mask);
	}

	return 0;
}

void
cpu_intr_ack(unsigned int intrs)
{

	/*
	 * ACK interrupts on PIC
	 */
	__asm__ __volatile(
	    "movb $0x20, %%al\n"
	    "outb %%al, $0xa0\n"
	    "outb %%al, $0x20\n"
	    ::: "al");
}
