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

/*
 * TODO: maybe this doesn't have to be board-specific expect for loadmme()?
 * IIRC NetBSD has a copypasted initarm() but Linux has a unified one,
 * and since our requirements are much simpler than those of NetBSD/Linux,
 * a common one should definitely work out.
 */

#include <hw/types.h>
#include <hw/kernel.h>

#include <bmk-core/core.h>
#include <bmk-core/mainthread.h>
#include <bmk-core/pgalloc.h>
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <bmk-core/sched.h>
#include <bmk-core/string.h>

#include <integrator/boardreg.h>

static void
loadmem(void)
{
	extern char _end[];
	unsigned long osend = bmk_round_page((unsigned long)_end);
	int meminfo = (*(unsigned long *)CM_SDRAM & 0x1f) >> 2;
	int memend;

	bmk_assert(meminfo <= 4);
	memend = (1<<meminfo) * 16*1024*1024;

	bmk_pgalloc_loadmem(osend, memend);
	bmk_memsize = memend - osend;
}

static void
clockinit(void)
{

	/* XXX: timer will wrap in ~10 days, FIXXME */
	outl(TMR1_CTRL, TMR_CTRL_EN | TMR_CTRL_D256 | TMR_CTRL_32);
	outl(TMR1_CLRINT, 0);
}

static char cmdline[] = "\n"
"{\n"
"	cmdline: \"HELLO just dropping by\n\",\n"
"	net :  {\n"
"		\"if\":		\"sm0\",\n"
"		\"type\":	\"inet\",\n"
"		\"method\":	\"static\",\n"
"		\"addr\":	\"10.0.0.2\",\n"
"		\"mask\":	\"24\",\n"
"	}\n"
"}\n";

void
arm_boot(void)
{
	extern char vector_start[], vector_end[];

	bmk_memcpy((void *)0, vector_start, vector_end - vector_start);

	bmk_printf_init(cons_putc, NULL);
	bmk_printf("rump kernel bare metal bootstrap (ARM)\n\n");

	bmk_sched_init();

	bmk_core_init(BMK_THREAD_STACK_PAGE_ORDER);
	loadmem();

	clockinit();
	intr_init();

	spl0();

	bmk_sched_startmain(bmk_mainthread, cmdline);
}

unsigned long bmk_cpu_arm_curtcb;

void
arm_undefined(unsigned long *trapregs)
{
	unsigned long instaddr = *(trapregs);
	unsigned long instr = *(unsigned long *)(instaddr-4);
	int reg = (instr >> 12) & 0xf;

	if ((instr & 0xffff0fff) == 0xee1d0f70 && reg <= 12) {
		*((trapregs-14)+reg) = bmk_cpu_arm_curtcb;
	} else {
		bmk_platform_halt("undefined instruction");
	}
}

/* no interrupt handler for the timer, so we just clear it */
static unsigned int clearmask = 0x80;

/* dynamically registered interrupts */
static unsigned int intmask;

void
arm_interrupt(unsigned long *trapregs)
{
	uint32_t intstat = inl(INTR_STATUS);
	uint32_t clearint = intstat & clearmask;

	splhigh();

	if (clearint) {
		outl(INTR_CLEAR, clearint);
		intstat &= ~clearint;
	}

	if (intstat) {
		outl(INTR_CLEAR, intstat);
		isr(intstat);
	}

	spl0();
}

void
bmk_platform_cpu_sched_settls(struct bmk_tcb *next)
{

	bmk_cpu_arm_curtcb = next->btcb_tp;
#ifdef notonthisarm
	asm volatile("mcr p15, 0, %0, c13, c0, 2" :: "r"(next->btcb_tp));
#endif
}

/* timer is 1MHz, we use divisor 256 */
#define NSEC_PER_TICK ((1000*1000*1000ULL)/(1000*1000/256))

bmk_time_t
bmk_platform_cpu_clock_monotonic(void)
{

	return ~inl(TMR1_VALUE) * NSEC_PER_TICK;
}

bmk_time_t
bmk_platform_cpu_clock_epochoffset(void)
{

	return 0;
}

void
bmk_platform_cpu_block(bmk_time_t until)
{

	outl(TMR2_CTRL, TMR_CTRL_EN | TMR_CTRL_IE);
	outl(TMR2_CLRINT, 0);

	outl(INTR_ENABLE, 0x80);
	spl0();
	asm volatile(
	    "mov r0, #0x0\n"
	    "mcr p15, 0, r0, c7, c0, 4\n" ::: "r0");
	splhigh();
	outl(INTR_CLEAR, 0x80);
}

int
cpu_intr_init(int intr)
{

	intmask |= 1<<intr;
	outl(INTR_ENABLE, intmask);
	return 0;
}

void
cpu_intr_ack(unsigned mask)
{

	outl(INTR_ENABLE, mask);
}
