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

#include <hw/types.h>
#include <hw/kernel.h>

#include <integrator/boardreg.h>

/*
 * Since this is serial, unlike in a framebuffer console,
 * we can just write a character and that's it.  NetBSD seems to use
 * a scheme where it waits for the FIFO to be empty before writing to
 * it, and also after writing to it.  Not sure why you need to wait
 * after writing.  Therefore, we just wait before writing.
 */
void
cons_putc(int c)
{
	volatile uint8_t *const uartdr = (uint8_t *)(UART0 + UARTDR);
	volatile uint16_t *const uartfr = (uint16_t *)(UART0 + UARTFR);
	int timo;
	unsigned char cw = c;

	timo = 150000; /* magic value from NetBSD */
	while (!(*uartfr & UARTFR_TXFE) && --timo)
		continue;

	*uartdr = cw;
}

void
cons_puts(const char *s)
{
	int c;

	while ((c = *s++) != 0)
		cons_putc(c);
}
