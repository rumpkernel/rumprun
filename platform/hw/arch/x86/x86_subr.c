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

void bmk_cpu_isr_9(void);
void bmk_cpu_isr_10(void);
void bmk_cpu_isr_11(void);
void bmk_cpu_isr_14(void);
void bmk_cpu_isr_15(void);

static int pic2mask = 0xff;

int
bmk_cpu_intr_init(int intr)
{

	/* XXX: too lazy to keep PIC1 state */
	if (intr < 8)
		return BMK_EGENERIC;

#define FILLGATE(n) case n: bmk_cpu_fillgate(32+n, bmk_cpu_isr_##n, 0); break
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
	pic2mask &= ~(1<<(intr-8));
	outb(PIC2_DATA, pic2mask);

	return 0;
}

void
bmk_cpu_initpic(void)
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
	outb(PIC2_DATA, pic2mask);
}

void
bmk_cpu_intr_ack(void)
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

bmk_time_t
bmk_cpu_clock_now(void)
{
	uint64_t val;
	unsigned long eax, edx;

	/* um um um */
	__asm__ __volatile__("rdtsc" : "=a"(eax), "=d"(edx));
	val = ((uint64_t)edx<<32)|(eax);

	/* just approximate that 1 cycle = 1ns.  "good enuf" for now */
	return val;
}

void
bmk_cpu_nanohlt(void)
{

	/*
	 * Enable clock interrupt and wait for the next whichever interrupt
	 */
	outb(PIC1_DATA, 0xff & ~(1<<2|1<<0));
	hlt();
	outb(PIC1_DATA, 0xff & ~(1<<2));
}
