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
#include <bmk/kernel.h>

#define CONS_WIDTH 80
#define CONS_HEIGHT 25
#define CONS_MAGENTA 0x500
static volatile uint16_t *cons_buf = (volatile uint16_t *)0xb8000;

static void
cons_putat(int c, int x, int y)
{

	cons_buf[x + y*CONS_WIDTH] = CONS_MAGENTA|c;
}

/* display a character in the next available slot */
void
bmk_cons_putc(int c)
{
	static int cons_x;
	static int cons_y;
	int x;
	int doclear = 0;

	if (c == '\n') {
		cons_x = 0;
		cons_y++;
		doclear = 1;
	} else if (c == '\r') {
		cons_x = 0;
	} else if (c == '\t') {
		cons_x = (cons_x+8) & ~7;
	} else {
		cons_putat(c, cons_x++, cons_y);
	}
	if (cons_x == CONS_WIDTH) {
		cons_x = 0;
		cons_y++;
		doclear = 1;
	}
	if (cons_y == CONS_HEIGHT) {
		cons_y--;
		/* scroll screen up one line */
		for (x = 0; x < (CONS_HEIGHT-1)*CONS_WIDTH; x++)
			cons_buf[x] = cons_buf[x+CONS_WIDTH];
	}
	if (doclear) {
		for (x = 0; x < CONS_WIDTH; x++)
			cons_putat(' ', x, cons_y);
	}
}

void
bmk_cons_clear(void)
{
	int x;

	for (x = 0; x < CONS_HEIGHT * CONS_WIDTH; x++)
		cons_putat(' ', x % CONS_WIDTH, x / CONS_WIDTH);
}
