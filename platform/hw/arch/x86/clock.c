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

/* clock isr trampoline (in locore.S) */
void bmk_cpu_isr_clock(void);

void
bmk_x86_initclocks(void)
{

	/*
	 * map clock interrupt.
	 * note, it's still disabled in the PIC, we only enable it
	 * during nanohlt
	 */
	bmk_x86_fillgate(32, bmk_cpu_isr_clock, 0);

	/* initialize the timer to 100Hz */
	outb(TIMER_MODE, TIMER_RATEGEN | TIMER_16BIT);
	outb(TIMER_CNTR, TIMER_HZ/HZ & 0xff);
	outb(TIMER_CNTR, TIMER_HZ/HZ >> 8);
}

void
bmk_isr_clock(void)
{

	/* nada */
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
