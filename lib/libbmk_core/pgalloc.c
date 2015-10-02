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
#include <bmk-core/string.h>

#include <bmk-pcpu/pcpu.h>

#ifndef BMK_PGALLOC_DEBUG
#define DPRINTF(x)
#else
#define DPRINTF(x) bmk_printf x
#endif

/*
 * The allocation bitmap is offset to the first page loaded, which is
 * nice if someone loads memory starting in high ranges.  Notably,
 * we don't need a pg->va operation since allocation is always done
 * through the freelists in va, and the pgmap is used only as a lookup
 * table for coalescing entries when pages are freed.
 */
static unsigned long minpage_addr, maxpage_addr;
#define va_to_pg(x) (((unsigned long)x - minpage_addr)>>BMK_PCPU_PAGE_SHIFT)

/*
 * ALLOCATION BITMAP
 *  One bit per page of memory. Bit set => page is allocated.
 */

static unsigned long *alloc_bitmap;
#define PAGES_PER_MAPWORD (sizeof(unsigned long) * 8)

#define allocated_in_map(_pn) \
  (((_pn) >= (minpage_addr << BMK_PCPU_PAGE_SHIFT)) && \
   ((_pn) <  (maxpage_addr << BMK_PCPU_PAGE_SHIFT)) && \
  alloc_bitmap[(_pn)/PAGES_PER_MAPWORD] & (1UL<<((_pn)&(PAGES_PER_MAPWORD-1))))

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

typedef struct chunk_head_st chunk_head_t;
typedef struct chunk_tail_st chunk_tail_t;

struct chunk_head_st {
	chunk_head_t  *next;
	chunk_head_t **pprev;
	int            level;
};

struct chunk_tail_st {
	int level;
};

/* Linked lists of free chunks of different powers-of-two in size. */
#define FREELIST_SIZE ((sizeof(void*)<<3)-BMK_PCPU_PAGE_SHIFT)
static chunk_head_t **free_head;
static chunk_head_t  *free_tail;
#define FREELIST_EMPTY(_l) ((_l)->next == NULL)

#ifdef BMK_PGALLOC_DEBUG
/* XXX: this debug code hasn't been compiled in this reincarnation */
/*
 * Prints allocation[0/1] for @nr_pages, starting at @start
 * address (virtual).
 */
static void __attribute__((used))
print_allocation(void *start, int nr_pages)
{
	unsigned long pfn_start = va_to_pg(start);
	int count;

	for (count = 0; count < nr_pages; count++) {
		if (allocated_in_map(pfn_start + count))
			bmk_printf("1");
		else
			bmk_printf("0");
	}

	bmk_printf("\n");
}

/*
 * Prints chunks (making them with letters) for @nr_pages starting
 * at @start (virtual).
 */
#define MAXCHUNKS 1024
static void __attribute__((used))
print_chunks(void *start, int nr_pages)
{
	char chunks[MAXCHUNKS+1], current='A';
	unsigned order, count;
	chunk_head_t *head;
	unsigned long pfn_start = va_to_pg(start);

	bmk_memset(chunks, (int)'_', MAXCHUNKS);
	if (nr_pages > MAXCHUNKS) {
		bmk_printf("%s: can only print %u pages."
		    "Increase buffer size\n", __func__, MAXCHUNKS);
	}

	for (order=0; order < FREELIST_SIZE; order++) {
		head = free_head[order];
		while (!FREELIST_EMPTY(head)) {
			unsigned long headva;

			headva = va_to_pg(head);
			for (count = 0; count < 1UL<< head->level; count++) {
				if(count + headva - pfn_start < 1000)
					chunks[count + headva - pfn_start]
					    = current;
			}
			head = head->next;
			current++;
		}
	}
	chunks[nr_pages] = '\0';
	bmk_printf("%s\n", chunks);
}
#endif


/*
 * Initialise allocator, placing addresses [@min,@max] in free pool.
 * @min and @max are PHYSICAL addresses.
 */
void
bmk_pgalloc_loadmem(unsigned long min, unsigned long max)
{
	static int called;
	unsigned long range, bitmap_size;
	chunk_head_t *ch;
	chunk_tail_t *ct;
	unsigned int i;

	if (called)
		bmk_platform_halt("bmk_pgalloc_loadmem called more than once");
	called = 1;

	bmk_assert(max > min);

	/*
	 * XXX: allocate dynamically so that we don't have to know
	 * PAGE_SIZE at compile-time.  FIXXXME
	 */
	free_head = (void *)min;
	min += FREELIST_SIZE * sizeof(*free_head);
	free_tail = (void *)min;
	min += FREELIST_SIZE * sizeof(*free_tail);

	min = bmk_round_page(min);
	max = bmk_trunc_page(max);
	maxpage_addr = max;

	for (i = 0; i < FREELIST_SIZE; i++) {
		free_head[i]       = &free_tail[i];
		free_tail[i].pprev = &free_head[i];
		free_tail[i].next  = NULL;
	}

	/* Allocate space for the allocation bitmap. */
	bitmap_size  = (max+1) >> (BMK_PCPU_PAGE_SHIFT+3);
	bitmap_size  = bmk_round_page(bitmap_size);
	alloc_bitmap = (unsigned long *)min;
	min         += bitmap_size;
	minpage_addr = min;
	range        = max - min;

	/* All allocated by default. */
	bmk_memset(alloc_bitmap, ~0, bitmap_size);
	/* Free up the memory we've been given to play with. */
	map_free((void *)min, range>>BMK_PCPU_PAGE_SHIFT);

	while (range != 0) {
		/*
		 * Next chunk is limited by alignment of min, but also
		 * must not be bigger than remaining range.
		 */
		for ( i = BMK_PCPU_PAGE_SHIFT; (1UL<<(i+1)) <= range; i++ )
			if (min & (1UL<<i))
				break;

		ch = (chunk_head_t *)min;

		min   += (1UL<<i);
		range -= (1UL<<i);

		ct = (chunk_tail_t *)min-1;
		i -= BMK_PCPU_PAGE_SHIFT;
		ch->level       = i;
		ch->next        = free_head[i];
		ch->pprev       = &free_head[i];
		ch->next->pprev = &ch->next;
		free_head[i]    = ch;
		ct->level       = i;
	}
}

/* Allocate 2^@order contiguous pages. Returns a VIRTUAL address. */
void *
bmk_pgalloc(int order)
{
	unsigned int i;
	chunk_head_t *alloc_ch, *spare_ch;
	chunk_tail_t            *spare_ct;

	/* Find smallest order which can satisfy the request. */
	for (i = order; i < FREELIST_SIZE; i++) {
		if (!FREELIST_EMPTY(free_head[i]))
			break;
	}
	if (i == FREELIST_SIZE) {
		bmk_printf("cannot handle page request order %d!\n", order);
		return 0;
	}

	/* Unlink a chunk. */
	alloc_ch = free_head[i];
	free_head[i] = alloc_ch->next;
	alloc_ch->next->pprev = alloc_ch->pprev;

	/* We may have to break the chunk a number of times. */
	while (i != (unsigned)order) {
		/* Split into two equal parts. */
		i--;
		spare_ch = (chunk_head_t *)((char *)alloc_ch
		    + (1UL<<(i+BMK_PCPU_PAGE_SHIFT)));
		spare_ct = (chunk_tail_t *)((char *)spare_ch
		    + (1UL<<(i+BMK_PCPU_PAGE_SHIFT)))-1;

		/* Create new header for spare chunk. */
		spare_ch->level = i;
		spare_ch->next  = free_head[i];
		spare_ch->pprev = &free_head[i];
		spare_ct->level = i;

		/* Link in the spare chunk. */
		spare_ch->next->pprev = &spare_ch->next;
		free_head[i] = spare_ch;
	}

	map_alloc(alloc_ch, 1UL<<order);
	DPRINTF(("bmk_pgalloc: allocated 0x%lx bytes at %p\n",
	    1UL<<(order+BMK_PCPU_PAGE_SHIFT), alloc_ch));

	return alloc_ch;
}

void
bmk_pgfree(void *pointer, int order)
{
	chunk_head_t *freed_ch, *to_merge_ch;
	chunk_tail_t *freed_ct;
	unsigned long mask;

	DPRINTF(("bmk_pgfree: freeing 0x%lx bytes at %p\n",
	    1UL<<(order+BMK_PCPU_PAGE_SHIFT), pointer));

	/* First free the chunk */
	map_free(pointer, 1UL << order);

	/* Create free chunk */
	freed_ch = pointer;
	freed_ct = (chunk_tail_t *)((char *)pointer
	    + (1UL<<(order + BMK_PCPU_PAGE_SHIFT)))-1;

	/* Now, possibly we can conseal chunks together */
	while ((unsigned)order < FREELIST_SIZE) {
		mask = 1UL << (order + BMK_PCPU_PAGE_SHIFT);
		if ((unsigned long)freed_ch & mask) {
			to_merge_ch = (chunk_head_t *)((char *)freed_ch - mask);
			if (allocated_in_map(va_to_pg(to_merge_ch))
			    || to_merge_ch->level != order)
				break;

			/* Merge with predecessor */
			freed_ch = to_merge_ch;
		} else {
			to_merge_ch = (chunk_head_t *)((char *)freed_ch + mask);
			if (allocated_in_map(va_to_pg(to_merge_ch))
			    || to_merge_ch->level != order)
				break;

			/* Merge with successor */
			freed_ct = (chunk_tail_t *)((char *)to_merge_ch+mask)-1;
		}

		/* We are commited to merging, unlink the chunk */
		*(to_merge_ch->pprev) = to_merge_ch->next;
		to_merge_ch->next->pprev = to_merge_ch->pprev;

		order++;
	}

	/* Link the new chunk */
	freed_ch->level = order;
	freed_ch->next  = free_head[order];
	freed_ch->pprev = &free_head[order];
	freed_ct->level = order;

	freed_ch->next->pprev = &freed_ch->next;
	free_head[order] = freed_ch;

}

#if 0
void
sanity_check(void)
{
	unsigned int x;
	chunk_head_t *head;

	for (x = 0; x < FREELIST_SIZE; x++) {
		for (head = free_head[x];
		    !FREELIST_EMPTY(head);
		    head = head->next) {
			bmk_assert(!allocated_in_map(va_to_pg(head)));
			if (head->next)
				ASSERT(head->next->pprev == &head->next);
		}
		if (free_head[x]) {
			ASSERT(free_head[x]->pprev == &free_head[x]);
		}
	}
}
#endif
