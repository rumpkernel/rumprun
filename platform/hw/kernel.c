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

#include <bmk/types.h>
#include <bmk/multiboot.h>
#include <bmk/kernel.h>

#include <bmk-core/core.h>
#include <bmk-core/string.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/pgalloc.h>
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <bmk-core/queue.h>
#include <bmk-core/sched.h>

unsigned long bmk_membase;
unsigned long bmk_memsize;

unsigned long
bmk_platform_memsize(void)
{

	return bmk_memsize;
}

void
bmk_platform_block(bmk_time_t until)
{
	int s = bmk_spldepth;

	/* enable interrupts around the sleep */
	if (bmk_spldepth) {
		bmk_spldepth = 1;
		spl0();
	}
	bmk_cpu_nanohlt();
	if (s) {
		splhigh();
		bmk_spldepth = s;
	}
}

/*
 * splhigh()/spl0() internally track depth
 */
unsigned long
bmk_platform_splhigh(void)
{

	splhigh();
	return 0;
}

void
bmk_platform_splx(unsigned long x)
{

	spl0();
}
 
void
bmk_run(char *cmdline)
{

	bmk_sched_startmain(bmk_mainthread, cmdline);
}

void __attribute__((noreturn))
bmk_platform_halt(const char *panicstring)
{

	if (panicstring)
		bmk_printf("PANIC: %s\n", panicstring);
	bmk_printf("halted (well, spinning ...)\n");
	for (;;)
		continue;
}
