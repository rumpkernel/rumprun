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
static size_t currentpg;
#define MAXPAGEALIGN (1<<BMK_THREAD_STACK_PAGE_ORDER)
void *
bmk_allocpg(size_t howmany)
{
	unsigned long rv;

	rv = bmk_membase + PAGE_SIZE*currentpg;
	currentpg += howmany;
	if (currentpg*PAGE_SIZE > bmk_memsize)
		return NULL;

	return (void *)rv;
}

/*
 * Allocate a 2^n chunk of pages, aligned at 2^n.  This is currently
 * for the benefit of thread stack allocation, and should be going
 * away in some time when the migration to TLS is complete.
 */
static void *
alignedpgalloc(int shift)
{
	struct stackcache *sc;
	int align = 1<<shift;
	size_t alignedoff;
	void *rv;

	if (shift == BMK_THREAD_STACK_PAGE_ORDER &&
	    (sc = LIST_FIRST(&cacheofstacks)) != NULL) {
		LIST_REMOVE(sc, sc_entries);
		return sc;
	}

	if (align > MAXPAGEALIGN)
		align = MAXPAGEALIGN;

	/* need to leave this much space until the next aligned alloc */
	alignedoff = (bmk_membase + currentpg*PAGE_SIZE) % (align*PAGE_SIZE);
	if (alignedoff)
		currentpg += align - (alignedoff>>PAGE_SHIFT);

	rv = bmk_allocpg(1<<shift);
	if (((unsigned long)rv & (align*PAGE_SIZE-1)) != 0) {
		bmk_printf("wanted %d aligned, got memory at %p\n",
		    align, rv);
		bmk_platform_halt("fail");
	}
	return rv;
}

void *
bmk_platform_allocpg2(int shift)
{

	return alignedpgalloc(shift);
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

	bmk_assert((bmk_membase & (PAGE_SIZE-1)) == 0);

	return 0;
}

void
bmk_main(struct multiboot_info *mbi)
{
	static char cmdline[2048];

	bmk_printf_init(bmk_cons_putc, NULL);
	bmk_core_init(BMK_THREAD_STACK_PAGE_ORDER, PAGE_SIZE);

	if (bmk_strlen((char *)mbi->cmdline) > sizeof(cmdline)-1)
		bmk_platform_halt("command line too long"); /* XXX */
	bmk_memcpy(cmdline, (char *)mbi->cmdline, sizeof(cmdline));

	bmk_printf("rump kernel bare metal bootstrap\n\n");
	if ((mbi->flags & MULTIBOOT_MEMORY_INFO) == 0) {
		bmk_printf("multiboot memory info not available\n");
		return;
	}
	if (parsemem(mbi->mmap_addr, mbi->mmap_length))
		return;
	bmk_cpu_init();
	bmk_isr_init();

	/* enough bootstrap already, jump to main thread */
	bmk_sched_init(bmk_mainthread, cmdline);
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
