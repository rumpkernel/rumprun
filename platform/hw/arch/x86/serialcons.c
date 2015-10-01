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

static uint16_t combase = 0;

void
serialcons_init(uint16_t combase_init, int speed)
{
	uint16_t divisor = 115200 / speed;

	combase = combase_init;
	outb(combase + COM_IER, 0x00);
	outb(combase + COM_LCTL, 0x80);
	outb(combase + COM_DLBL, divisor & 0xff);
	outb(combase + COM_DLBH, divisor >> 8);
	outb(combase + COM_LCTL, 0x03);
	outb(combase + COM_FIFO, 0xc7);
}

void
serialcons_putc(int c)
{

	if (!combase)
		return;
	if (c == '\n')
		serialcons_putc('\r');

	/*
	 * Write a single character at a time, while the output FIFO has space.
	 */
	while ((inb(combase + COM_LSR) & 0x20) == 0)
		;
	outb(combase + COM_DATA, c);
}
