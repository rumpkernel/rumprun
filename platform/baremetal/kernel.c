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
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <bmk-core/queue.h>
#include <bmk-core/sched.h>

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
	bmk_printf("baremetal halted (well, spinning ...)\n");
	for (;;)
		continue;
}
