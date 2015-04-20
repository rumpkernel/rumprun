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
#include <bmk-core/memalloc.h>
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <bmk-core/queue.h>

#include <bmk-base/netbsd_initfini.h>

unsigned long bmk_membase;
unsigned long bmk_memsize;

LIST_HEAD(, stackcache) cacheofstacks = LIST_HEAD_INITIALIZER(cacheofstacks);
struct stackcache {
	void *sc_stack;
	LIST_ENTRY(stackcache) sc_entries;
};

/*
 * We don't need freepg.
 *
 * For the humour impaired: it was a joke, on the TODO ... but really,
 * it's not that urgent since the rump kernel uses its own caching
 * allocators, so once the backing pages are allocated, they tend to
 * never get freed.  The only thing that in practical terms gets
 * deallocated is thread stacks, and for now we simply cache those
 * as a special case. (nb. even that holds only for native thread stacks,
 * not pthread stacks).
 */
void *
bmk_allocpg(size_t howmany)
{
	struct stackcache *sc;
	static size_t current = 0;
	unsigned long rv;

	if (howmany == 1<<BMK_THREAD_STACK_PAGE_ORDER &&
	    (sc = LIST_FIRST(&cacheofstacks)) != NULL) {
		LIST_REMOVE(sc, sc_entries);
		return sc;
	}

	rv = bmk_membase + PAGE_SIZE*current;
	current += howmany;
	if (current*PAGE_SIZE > bmk_memsize)
		return NULL;

	return (void *)rv;
}

void *
bmk_platform_allocpg2(int shift)
{

	return bmk_allocpg(1<<shift);
}

void
bmk_platform_freepg2(void *mem, int shift)
{

	if (shift == BMK_THREAD_STACK_PAGE_ORDER) {
		struct stackcache *sc = mem;

		LIST_INSERT_HEAD(&cacheofstacks, sc, sc_entries);
		return;
	}

	bmk_printf("WARNING: freepg2 called! (%p, %d)\n", mem, shift);
}

void
bmk_platform_block(bmk_time_t until)
{

	bmk_cpu_nanohlt();
}

unsigned long
bmk_platform_splhigh(void)
{

	return 0; /* XXX */
}

void
bmk_platform_splx(unsigned long x)
{

	return; /* XXX */
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

static void
bmk_mainthread(void)
{

	_netbsd_init();
	bmk_beforemain();
	_netbsd_fini();
}

void
bmk_main(struct multiboot_info *mbi)
{

	bmk_printf_init(bmk_cons_putc, NULL);
	bmk_core_init(BMK_THREAD_STACK_PAGE_ORDER, PAGE_SIZE);

	bmk_printf("rump kernel bare metal bootstrap\n\n");
	if ((mbi->flags & MULTIBOOT_MEMORY_INFO) == 0) {
		bmk_printf("multiboot memory info not available\n");
		return;
	}
	if (parsemem(mbi->mmap_addr, mbi->mmap_length))
		return;
	bmk_cpu_init();
	bmk_isr_init();

	/* enough already, jump to main thread */
	bmk_sched_init(bmk_mainthread);
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
bmk_platform_halt(const char *panicstring)
{

	if (panicstring)
		bmk_printf("PANIC: %s\n", panicstring);
	bmk_printf("baremetal halted (well, spinning ...)\n");
	for (;;)
		continue;
}
