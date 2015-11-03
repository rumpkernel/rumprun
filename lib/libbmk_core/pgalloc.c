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

/*-
 ****************************************************************************
 * (C) 2003 - Rolf Neugebauer - Intel Research Cambridge
 * (C) 2005 - Grzegorz Milos - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: mm.c
 *      Author: Rolf Neugebauer (neugebar@dcs.gla.ac.uk)
 *     Changes: Grzegorz Milos
 *
 *        Date: Aug 2003, chages Aug 2005
 *
 * Environment: Xen Minimal OS
 * Description: memory management related functions
 *              contains buddy page allocator from Xen.
 *
 ****************************************************************************
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <bmk-core/core.h>
#include <bmk-core/null.h>
#include <bmk-core/pgalloc.h>
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <bmk-core/queue.h>
#include <bmk-core/string.h>

#include <bmk-pcpu/pcpu.h>

#ifndef BMK_PGALLOC_DEBUG
#define DPRINTF(x)
#define SANITY_CHECK()
#else
int bmk_pgalloc_debug = 0;
#define DPRINTF(x) if (bmk_pgalloc_debug) bmk_printf x
#define SANITY_CHECK() sanity_check()
#endif

unsigned long pgalloc_totalkb, pgalloc_usedkb;

/*
 * The allocation bitmap is offset to the first page loaded, which is
 * nice if someone loads memory starting in high ranges.  Notably,
 * we don't need a pg->va operation since allocation is always done
 * through the freelists in va, and the pgmap is used only as a lookup
 * table for coalescing entries when pages are freed.
 */
static void *minpage_addr, *maxpage_addr;
#define va_to_pg(x) \
    (((unsigned long)x - (unsigned long)minpage_addr)>>BMK_PCPU_PAGE_SHIFT)

#define addr2chunk(_addr_,_offset_) \
    ((struct chunk *)(((char *)_addr_)+(_offset_)))

#define order2size(_order_) (1UL<<(_order_ + BMK_PCPU_PAGE_SHIFT))

/*
 * ALLOCATION BITMAP
 *  One bit per page of memory. Bit set => page is allocated.
 */

static unsigned long *alloc_bitmap;
#define PAGES_PER_MAPWORD (sizeof(unsigned long) * 8)
bmk_ctassert((PAGES_PER_MAPWORD & (PAGES_PER_MAPWORD-1)) == 0);

static int
addr_is_managed(void *addr)
{

	return addr >= minpage_addr && addr < maxpage_addr;
}

static int
allocated_in_map(void *addr)
{
	unsigned long pagenum;

	bmk_assert(addr_is_managed(addr));
	pagenum = va_to_pg(addr);
	return (alloc_bitmap[pagenum/PAGES_PER_MAPWORD] \
	    & (1UL<<(pagenum&(PAGES_PER_MAPWORD-1)))) != 0;
}

/*
 * Hint regarding bitwise arithmetic in map_{alloc,free}:
 *  -(1<<n)  sets all bits >= n.
 *  (1<<n)-1 sets all bits <  n.
 * Variable names in map_{alloc,free}:
 *  *_idx == Index into `alloc_bitmap' array.
 *  *_off == Bit offset within an element of the `alloc_bitmap' array.
 */

#define PAGES_TO_MAPOPVARS(va, np)					\
	unsigned long start, end, curr_idx, end_idx;			\
	unsigned long first_page = va_to_pg(va);			\
	curr_idx= first_page / PAGES_PER_MAPWORD;			\
	start	= first_page & (PAGES_PER_MAPWORD-1);			\
	end_idx	= (first_page + nr_pages) / PAGES_PER_MAPWORD;		\
	end	= (first_page + nr_pages) & (PAGES_PER_MAPWORD-1);

static void
map_alloc(void *virt, unsigned long nr_pages)
{
	PAGES_TO_MAPOPVARS(virt, nr_pages);

	if (curr_idx == end_idx) {
		alloc_bitmap[curr_idx] |= ((1UL<<end)-1) & -(1UL<<start);
	} else {
		alloc_bitmap[curr_idx] |= -(1UL<<start);
		while (++curr_idx < end_idx)
			alloc_bitmap[curr_idx] = ~0UL;
		alloc_bitmap[curr_idx] |= (1UL<<end)-1;
	}
}

static void
map_free(void *virt, unsigned long nr_pages)
{
	PAGES_TO_MAPOPVARS(virt, nr_pages);

	if (curr_idx == end_idx) {
		alloc_bitmap[curr_idx] &= -(1UL<<end) | ((1UL<<start)-1);
	} else {
		alloc_bitmap[curr_idx] &= (1UL<<start)-1;
		while (++curr_idx != end_idx)
			alloc_bitmap[curr_idx] = 0;
		alloc_bitmap[curr_idx] &= -(1UL<<end);
	}
}

/*
 * BINARY BUDDY ALLOCATOR
 */

#define CHUNKMAGIC 0x11020217
struct chunk {
	int level;
	int magic;

	LIST_ENTRY(chunk) entries;
};

static int
chunklevel(struct chunk *ch)
{

	bmk_assert(ch->magic == CHUNKMAGIC);
	return ch->level;
}

/*
 * Linked lists of free chunks of different powers-of-two in size.
 * The assumption is that pointer size * NBBY = va size.  It's
 * a pretty reasonable assumption, except that we really don't need
 * that much address space.  The order of what most CPUs implement (48bits)
 * would be more than plenty.  But since the unused levels don't consume
 * much space, leave it be for now.
 */
#define FREELIST_LEVELS (8*(sizeof(void*))-BMK_PCPU_PAGE_SHIFT)
static LIST_HEAD(, chunk) freelist[FREELIST_LEVELS];

static void
freechunk_link(void *addr, int order)
{
	struct chunk *ch = addr;

	ch->level = order;
	ch->magic = CHUNKMAGIC;

	LIST_INSERT_HEAD(&freelist[order], ch, entries);
}

#ifdef BMK_PGALLOC_DEBUG
static void __attribute__((used))
print_allocation(void *start, unsigned nr_pages)
{
	unsigned long addr = (unsigned long)start;

	for (; nr_pages > 0; nr_pages--, addr += BMK_PCPU_PAGE_SIZE) {
		if (allocated_in_map((void *)addr))
			bmk_printf("1");
		else
			bmk_printf("0");
	}

	bmk_printf("\n");
}

static void
sanity_check(void)
{
	unsigned int x;
	struct chunk *head;

	for (x = 0; x < FREELIST_LEVELS; x++) {
		LIST_FOREACH(head, &freelist[x], entries) {
			bmk_assert(!allocated_in_map(head));
			bmk_assert(head->magic == CHUNKMAGIC);
		}
	}
}
#endif

void
bmk_pgalloc_dumpstats(void)
{
	struct chunk *ch;
	unsigned long remainingkb;
	unsigned i;

	remainingkb = pgalloc_totalkb - pgalloc_usedkb;
	bmk_printf("pgalloc total %ld kB, used %ld kB (remaining %ld kB)\n",
	    pgalloc_totalkb, pgalloc_usedkb, remainingkb);

	bmk_printf("available chunks:\n");
	for (i = 0; i < FREELIST_LEVELS; i++) {
		unsigned long chunks, levelhas;

		if (LIST_EMPTY(&freelist[i]))
			continue;

		chunks = 0;
		LIST_FOREACH(ch, &freelist[i], entries) {
			chunks++;
		}
		levelhas = chunks * (order2size(i)>>10);
		bmk_printf("%8ld kB: %8ld chunks, %12ld kB\t(%2ld%%)\n",
		    order2size(i)>>10, chunks, levelhas,
		    (100*levelhas)/remainingkb);
	}
}

static void
carverange(unsigned long addr, unsigned long range)
{
	struct chunk *ch;
	unsigned i, r;

	while (range) {
		/*
		 * Next chunk is limited by alignment of addr, but also
		 * must not be bigger than remaining range.
		 */
		i = __builtin_ctzl(addr);
		r = 8*sizeof(range) - (__builtin_clzl(range)+1);
		if (i > r) {
			i = r;
		}
		i -= BMK_PCPU_PAGE_SHIFT;

		ch = addr2chunk(addr, 0);
		freechunk_link(ch, i);
		addr += order2size(i);
		range -= order2size(i);

		DPRINTF(("bmk_pgalloc: carverange chunk 0x%lx at %p\n",
		    order2size(i), ch));
	}
}

/*
 * Load [min,max] as available addresses.
 */
void
bmk_pgalloc_loadmem(unsigned long min, unsigned long max)
{
	static int called;
	unsigned long range, bitmap_size;
	unsigned int i;

	if (called)
		bmk_platform_halt("bmk_pgalloc_loadmem called more than once");
	called = 1;

	bmk_assert(max > min);

	min = bmk_round_page(min);
	max = bmk_trunc_page(max);

	DPRINTF(("bmk_pgalloc_loadmem: available memory [0x%lx,0x%lx]\n",
	    min, max));

	for (i = 0; i < FREELIST_LEVELS; i++) {
		LIST_INIT(&freelist[i]);
	}

	/* Allocate space for the allocation bitmap. */
	bitmap_size  = ((max-min) >> (BMK_PCPU_PAGE_SHIFT+3)) + 1;
	bitmap_size  = bmk_round_page(bitmap_size);
	alloc_bitmap = (unsigned long *)min;
	min         += bitmap_size;
	range        = max - min;

	minpage_addr = (void *)min;
	maxpage_addr = (void *)max;

	pgalloc_totalkb = range >> 10;
	pgalloc_usedkb = 0;

	/* All allocated by default. */
	bmk_memset(alloc_bitmap, ~0, bitmap_size);
	/* Free up the memory we've been given to play with. */
	map_free((void *)min, range>>BMK_PCPU_PAGE_SHIFT);

	carverange(min, range);
}

/* can we allocate len w/ align from freelist index i? */
static struct chunk *
satisfies_p(int i, unsigned long align)
{
	struct chunk *ch;
	unsigned long p;

	LIST_FOREACH(ch, &freelist[i], entries) {
		p = (unsigned long)ch;
		if ((p & (align-1)) == 0)
			return ch;
	}

	return NULL;
}

void *
bmk_pgalloc(int order)
{

	return bmk_pgalloc_align(order, 1);
}

void *
bmk_pgalloc_align(int order, unsigned long align)
{
	struct chunk *alloc_ch;
	unsigned long p, len;
	unsigned int bucket;

	bmk_assert((align & (align-1)) == 0);
	bmk_assert((unsigned)order < FREELIST_LEVELS);

	for (bucket = order; bucket < FREELIST_LEVELS; bucket++) {
		if ((alloc_ch = satisfies_p(bucket, align)) != NULL)
			break;
	}
	if (!alloc_ch) {
		bmk_printf("cannot handle page request order %d/0x%lx!\n",
		    order, align);
		return 0;
	}
	/* Unlink the chunk. */
	LIST_REMOVE(alloc_ch, entries);

	bmk_assert(alloc_ch->magic == CHUNKMAGIC);
	alloc_ch->magic = 0;

	/*
	 * TODO: figure out if we can cheaply carve the block without
	 * using the best alignment.
	 */
	len = order2size(order);
	p = (unsigned long)alloc_ch;

	/* carve up leftovers (if any) */
	carverange(p+len, order2size(bucket) - len);

	map_alloc(alloc_ch, 1UL<<order);
	DPRINTF(("bmk_pgalloc: allocated 0x%lx bytes at %p\n",
	    order2size(order), alloc_ch));
	pgalloc_usedkb += len>>10;

#ifdef BMK_PGALLOC_DEBUG
	{
		unsigned npgs = 1<<order;
		char *p = (char *)alloc_ch;

		for (npgs = 1<<order; npgs; npgs--, p += BMK_PCPU_PAGE_SIZE) {
			bmk_assert(allocated_in_map(p));
		}
	}
#endif

	SANITY_CHECK();

	bmk_assert(((unsigned long)alloc_ch & (align-1)) == 0);
	return alloc_ch;
}

void
bmk_pgfree(void *pointer, int order)
{
	struct chunk *freed_ch, *to_merge_ch;
	unsigned long mask;

	DPRINTF(("bmk_pgfree: freeing 0x%lx bytes at %p\n",
	    order2size(order), pointer));

#ifdef BMK_PGALLOC_DEBUG
	{
		unsigned npgs = 1<<order;
		char *p = (char *)pointer;

		for (npgs = 1<<order; npgs; npgs--, p += BMK_PCPU_PAGE_SIZE) {
			bmk_assert(allocated_in_map(p));
		}
	}
#endif

	/* free the allocation in the bitmap */
	map_free(pointer, 1UL << order);
	pgalloc_usedkb -= order2size(order)>>10;

	/* create as large a free chunk as we can */
	for (freed_ch = pointer; (unsigned)order < FREELIST_LEVELS; ) {
		mask = order2size(order);
		if ((unsigned long)freed_ch & mask) {
			to_merge_ch = addr2chunk(freed_ch, -mask);
			if (!addr_is_managed(to_merge_ch)
			    || allocated_in_map(to_merge_ch)
			    || chunklevel(to_merge_ch) != order)
				break;
			freed_ch->magic = 0;

			/* merge with predecessor, point freed chuck there */
			freed_ch = to_merge_ch;
		} else {
			to_merge_ch = addr2chunk(freed_ch, mask);
			if (!addr_is_managed(to_merge_ch)
			    || allocated_in_map(to_merge_ch)
			    || chunklevel(to_merge_ch) != order)
				break;
			freed_ch->magic = 0;

			/* merge with successor, freed chuck already correct */
		}

		to_merge_ch->magic = 0;
		LIST_REMOVE(to_merge_ch, entries);

		order++;
	}

	freechunk_link(freed_ch, order);

	SANITY_CHECK();
}
