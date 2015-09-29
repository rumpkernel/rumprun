/*-
 * Copyright (c) 2015 Martin Lucina.  All Rights Reserved.
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

#include <hw/types.h>
#include <hw/kernel.h>

#include <arch/x86/cons.h>

#include <bmk-core/printf.h>

static void (*vcons_putc)(int) = vgacons_putc;

/*
 * Filled in by locore from BIOS data area.
 */
uint16_t bios_com1_base, bios_crtc_base;

void
cons_init(void)
{

	/*
	 * If the BIOS says no CRTC is present and a serial port is present,
	 * use the serial console. Otherwise use the VGA console.
	 */
	if (bios_crtc_base == 0 && bios_com1_base != 0) {
		serialcons_init(bios_com1_base, 115200);
		vcons_putc = serialcons_putc;
	}
	bmk_printf_init(vcons_putc, NULL);
}

void
cons_putc(int c)
{

	vcons_putc(c);
}

void
cons_puts(const char *s)
{
	int c;

	while ((c = *s++) != 0)
		vcons_putc(c);
}
