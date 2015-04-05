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
#include <bmk/sched.h>
#include <bmk/app.h>

#include <bmk-core/core.h>
#include <bmk-core/string.h>
#include <bmk-core/bmk_ops.h>
#include <bmk-core/memalloc.h>

#include <bmk-base/netbsd_initfini.h>

unsigned long bmk_membase;
unsigned long bmk_memsize;

/*
 * we don't need freepg
 * (for the humour impaired: it was a joke, on the TODO ... but really,
 * it's not that urgent since the rump kernel uses its own caching
 * allocators, so once the backing pages are allocated, they tend to
 * never get freed)
 */
void *
bmk_allocpg(size_t howmany)
{
	static size_t current = 0;
	unsigned long rv;

	rv = bmk_membase + PAGE_SIZE*current;
	current += howmany;
	if (current*PAGE_SIZE > bmk_memsize)
		return NULL;

	return (void *)rv;
}

static void *
bmk_allocpg2(int shift)
{

	return bmk_allocpg(1<<shift);
}

static int
parsemem(uint32_t addr, uint32_t len)
{
	struct multiboot_mmap_entry *mbm;
	unsigned long memsize;
	unsigned long ossize, osbegin, osend;
	extern char _end[], _begin[];
	uint32_t off;

	/*
	 * Look for our memory.  We assume it's just in one chunk
	 * starting at MEMSTART.
	 */
	for (off = 0; off < len; off += mbm->size + sizeof(mbm->size)) {
		mbm = (void *)(addr + off);
		if (mbm->addr == MEMSTART
		    && mbm->type == MULTIBOOT_MEMORY_AVAILABLE) {
			break;
		}
	}
	bmk_assert(off < len);

	memsize = mbm->len;
	osbegin = (unsigned long)_begin;
	osend = round_page((unsigned long)_end);
	ossize = osend - osbegin;

	bmk_membase = mbm->addr + ossize;
	bmk_memsize = memsize - ossize;

	return 0;
}

static const struct bmk_ops myops = {
	.bmk_allocpg2 = bmk_allocpg2,
	.bmk_halt = bmk_halt,
};

void
bmk_main(struct multiboot_info *mbi)
{

	bmk_core_init(BMK_THREAD_STACKSIZE, PAGE_SIZE, &myops);

	bmk_cons_puts("rump kernel bare metal bootstrap\n\n");
	if ((mbi->flags & MULTIBOOT_MEMORY_INFO) == 0) {
		bmk_cons_puts("multiboot memory info not available\n");
		return;
	}
	if (parsemem(mbi->mmap_addr, mbi->mmap_length))
		return;
	bmk_cpu_init();
	bmk_sched_init();
	bmk_isr_init();

	_netbsd_init();
	bmk_beforemain();
	_netbsd_fini();
}

/*
 * console.  quick, cheap, dirty, etc.
 * Should eventually keep an in-memory log.  printf-debugging is currently
 * a bit, hmm, limited.
 */
 
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
 
/* display a string */
void
bmk_cons_puts(const char *str)
{

	for (; *str; str++)
		bmk_cons_putc(*str);
}

/*
 * init.  currently just clears the console.
 * rest is done in bmk_main()
 */
void
bmk_init(void)
{
	int x;

	for (x = 0; x < CONS_HEIGHT * CONS_WIDTH; x++)
		cons_putat(' ', x % CONS_WIDTH, x / CONS_WIDTH);
}

void __attribute__((noreturn))
bmk_halt(const char *panicstring)
{

	if (panicstring) {
		bmk_cons_puts("PANIC: ");
		bmk_cons_puts(panicstring);
		bmk_cons_puts("\n");
	}
	bmk_cons_puts("baremetal halted (well, spinning ...)\n");
	for (;;)
		continue;
}
