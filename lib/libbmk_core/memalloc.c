/*-
 * Copyright (c) 2013, 2015 Antti Kantee.  All rights reserved.
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * malloc.c (Caltech) 2/21/82
 * Chris Kingsley, kingsley@cit-20.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small 
 * number of different sizes, and keeps free lists of each size.  Blocks that
 * don't exactly fit are passed up to the next larger size.  In this 
 * implementation, the available sizes are 2^n-4 (or 2^n-10) bytes long.
 * This is designed for use in a virtual memory environment.
 *
 * Modified for bmk by Antti Kantee over 30 years later.
 */

#include <bmk-core/core.h>
#include <bmk-core/null.h>
#include <bmk-core/string.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/platform.h>
#include <bmk-core/pgalloc.h>
#include <bmk-core/printf.h>
#include <bmk-core/queue.h>

#include <bmk-pcpu/pcpu.h>

/*
 * Header goes right before the allocated space and holds
 * information about the allocation.  Notably, we support
 * max 4gig alignment.  If you need more, use some other
 * allocator than malloc.
 */
struct memalloc_hdr {
	uint32_t	mh_alignpad;	/* padding for alignment */
	uint16_t	mh_magic;	/* magic number */
	uint8_t		mh_index;	/* bucket # */
	uint8_t		mh_who;		/* who allocated */
};

struct memalloc_freeblk {
	LIST_ENTRY(memalloc_freeblk) entries;
};
LIST_HEAD(freebucket, memalloc_freeblk);

#define	MAGIC		0xef		/* magic # on accounting info */
#define UNMAGIC		0x1221		/* magic # != MAGIC */
#define UNMAGIC2	0x2442		/* magic # != MAGIC/UNMAGIC */

#define MINSHIFT 5
#define	LOCALBUCKETS (BMK_PCPU_PAGE_SHIFT - MINSHIFT)
#define MINALIGN 16
static struct freebucket freebuckets[LOCALBUCKETS];

/*
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
 */
static unsigned nmalloc[LOCALBUCKETS];

/* not multicore */
#define malloc_lock()
#define malloc_unlock()

static void *
morecore(int bucket)
{
	void *rv;
	uint8_t *p;
	unsigned long sz;		/* size of desired block */
	unsigned long nblks;		/* how many blocks we get */

	sz = 1<<(bucket+MINSHIFT);
	nblks = BMK_PCPU_PAGE_SIZE / sz;
	bmk_assert(nblks > 1);

	if ((p = rv = bmk_pgalloc_one()) == NULL)
		return NULL;

	/*
	 * Add new memory allocated to that on
	 * free list for this hash bucket.  Return one block.
	 */
	while (--nblks) {
		struct memalloc_freeblk *frb;

		p += sz;
		frb = (void *)p;
		LIST_INSERT_HEAD(&freebuckets[bucket], frb, entries);
	}
	return rv;
}

void
bmk_memalloc_init(void)
{
	unsigned i;

	bmk_assert(BMK_PCPU_PAGE_SIZE > 0);
	for (i = 0; i < LOCALBUCKETS; i++) {
		LIST_INIT(&freebuckets[i]);
	}
}

static void *
bucketalloc(unsigned bucket)
{
	struct memalloc_freeblk *frb;

	malloc_lock();

	/*
	 * If nothing in hash bucket right now,
	 * request more memory from the system.
	 */
	if ((frb = LIST_FIRST(&freebuckets[bucket])) == NULL) {
		if ((frb = morecore(bucket)) == NULL) {
			malloc_unlock();
			return NULL;
		}
	} else {
		LIST_REMOVE(frb, entries);
	}

	nmalloc[bucket]++;
	malloc_unlock();

	return frb;
}

void *
bmk_memalloc(unsigned long nbytes, unsigned long align, enum bmk_memwho who)
{
	struct memalloc_hdr *hdr;
	void *rv;
	unsigned long allocbytes;
	unsigned bucket;
	unsigned long alignpad;

	if (align & (align-1))
		return NULL;
	if (align < MINALIGN)
		align = MINALIGN;
	bmk_assert(align <= (1UL<<31));

	/* need at least this many bytes plus header to satisfy alignment */
	allocbytes = nbytes + ((sizeof(*hdr) + (align-1)) & ~(align-1));

	/*
	 * Convert amount of memory requested into closest block size
	 * stored in hash buckets which satisfies request.
	 * Account for space used per block for accounting.
	 */
	if (allocbytes < 1<<MINSHIFT) {
		bucket = 0;
	} else {
		bucket = 8*sizeof(allocbytes)
		    - __builtin_clzl(allocbytes>>MINSHIFT);
		if ((allocbytes & (allocbytes-1)) == 0)
			bucket--;
	}

	/* handle with page allocator? */
	if (bucket >= LOCALBUCKETS) {
		hdr = bmk_pgalloc(bucket+MINSHIFT - BMK_PCPU_PAGE_SHIFT);
	} else {
		hdr = bucketalloc(bucket);
	}
	if (hdr == NULL)
		return NULL;

	/* align op before returned memory */
	rv = (void *)(((unsigned long)(hdr+1) + align - 1) & ~(align - 1));
	alignpad = (unsigned long)rv - (unsigned long)hdr;

#ifdef MEMALLOC_TESTING
	bmk_memset(hdr, MAGIC, alignpad);
#endif

	hdr = ((struct memalloc_hdr *)rv)-1;
	hdr->mh_magic = MAGIC;
	hdr->mh_index = bucket;
	hdr->mh_alignpad = alignpad;
	hdr->mh_who = who;

  	return rv;
}

void *
bmk_xmalloc_bmk(unsigned long howmuch)
{
	void *rv;

	rv = bmk_memalloc(howmuch, 0, BMK_MEMWHO_WIREDBMK);
	if (rv == NULL)
		bmk_platform_halt("xmalloc failed");
	return rv;
}

void *
bmk_memcalloc(unsigned long n, unsigned long size, enum bmk_memwho who)
{
	void *v;
	unsigned long tot = n * size;

	if (size != 0 && tot / size != n)
		return NULL;

	if ((v = bmk_memalloc(tot, MINALIGN, who)) != NULL) {
		bmk_memset(v, 0, tot);
	}
	return v;
}

void
bmk_memfree(void *cp, enum bmk_memwho who)
{   
	struct memalloc_hdr *hdr;
	struct memalloc_freeblk *frb;
	unsigned long alignpad;
	unsigned int index;
	void *origp;

  	if (cp == NULL)
  		return;
	hdr = ((struct memalloc_hdr *)cp)-1;
	if (hdr->mh_magic != MAGIC) {
#ifdef MEMALLOC_TESTING
		bmk_assert(0);
#else
		bmk_printf("bmk_memfree: invalid pointer %p\n", cp);
		return;
#endif
	}
	if (hdr->mh_who != who) {
		bmk_printf("bmk_memfree: mismatch %d vs. %d for %p",
		    hdr->mh_who, who, cp);
		bmk_platform_halt("bmk_memalloc error");
	}

	index = hdr->mh_index;
	alignpad = hdr->mh_alignpad;

	origp = (unsigned char *)cp - alignpad;

#ifdef MEMALLOC_TESTING
	{
		unsigned long i;

		for (i = 0;
		    (unsigned char *)origp + i < (unsigned char *)hdr;
		    i++) {
			bmk_assert(*((unsigned char *)origp + i) == MAGIC);

		}
	}
#endif

	if (index >= LOCALBUCKETS) {
		bmk_pgfree(origp, (index+MINSHIFT) - BMK_PCPU_PAGE_SHIFT);
	} else {
		malloc_lock();
		frb = origp;
		LIST_INSERT_HEAD(&freebuckets[index], frb, entries);
		nmalloc[index]--;
		malloc_unlock();
	}
}

/*
 * Don't do any of "storage compaction" nonsense, "just" the three modes:
 *   + cp == NULL ==> malloc
 *   + nbytes == 0 ==> free
 *   + else ==> realloc
 *
 * Also, assume that realloc() is always called from POSIX compat code,
 * because nobody sane would use realloc()
 */
void *
bmk_memrealloc_user(void *cp, unsigned long nbytes)
{   
	struct memalloc_hdr *hdr;
  	unsigned long size;
	unsigned long alignpad;
	void *np;

	if (cp == NULL)
		return bmk_memalloc(nbytes, MINALIGN, BMK_MEMWHO_USER);

	if (nbytes == 0) {
		bmk_memfree(cp, BMK_MEMWHO_USER);
		return NULL;
	}

	hdr = ((struct memalloc_hdr *)cp)-1;
	size = hdr->mh_index;
	alignpad = hdr->mh_alignpad;

	/* don't bother "compacting".  don't like it?  don't use realloc! */
	if (((1<<(size+MINSHIFT)) - alignpad) >= nbytes)
		return cp;

	/* we're gonna need a bigger bucket */
	np = bmk_memalloc(nbytes, 8, BMK_MEMWHO_USER);
	if (np == NULL)
		return NULL;

	bmk_memcpy(np, cp, (1<<(size+MINSHIFT)) - alignpad);
	bmk_memfree(cp, BMK_MEMWHO_USER);
	return np;
}

/*
 * mstats - print out statistics about malloc
 * 
 * Prints two lines of numbers, one showing the length of the free list
 * for each size category, the second showing the number of mallocs -
 * frees for each size category.
 */
void
bmk_memalloc_printstats(void)
{
	struct memalloc_freeblk *frb;
	unsigned long totfree = 0, totused = 0;
	unsigned int i, j;

	bmk_printf("Memory allocation statistics\nfree:\t");
	for (i = 0; i < LOCALBUCKETS; i++) {
		j = 0;
		LIST_FOREACH(frb, &freebuckets[i], entries) {
			j++;
		}
		bmk_printf(" %d", j);
		totfree += j * (1 << (i + MINSHIFT));
  	}
	bmk_printf("\nused:\t");
	for (i = 0; i < LOCALBUCKETS; i++) {
		bmk_printf(" %d", nmalloc[i]);
		totused += nmalloc[i] * (1 << (i + MINSHIFT));
  	}
	bmk_printf("\n\tTotal in use: %lukB, total free in buckets: %lukB\n",
	    totused/1024, totfree/1024);
}


/*
 * The rest of this file contains unit tests.
 */

#ifdef MEMALLOC_TESTING

#define TEST_SMALL_MINALLOC 0
#define TEST_SMALL_MAXALLOC (128)

#define TEST_LARGE_MINALLOC 0
#define TEST_LARGE_MAXALLOC (64*1024)

#define TEST_MINALIGN 1
#define TEST_MAXALIGN 16

#define NALLOC 1024
#define NRING 16

static unsigned randstate;

static unsigned
myrand(void)
{

	return (randstate = randstate * 1103515245 + 12345) % (0x80000000L);
}

static void *
testalloc(unsigned long min, unsigned long max)
{
	void *v, *nv;
	unsigned int size1, size2, align;

	/* doesn't give an even bucket distribution, but ... */
	size1 = myrand() % ((max-min)+1) + min;
	align = myrand() % ((TEST_MAXALIGN-TEST_MINALIGN)+1) + TEST_MINALIGN;

	v = bmk_memalloc(size1, 1<<align, BMK_MEMWHO_USER);
	if (!v)
		return NULL;
	bmk_assert(((uintptr_t)v & (align-1)) == 0);
	bmk_memset(v, UNMAGIC, size1);

	size2 = myrand() % ((max-min)+1) + min;
	nv = bmk_memrealloc_user(v, size2);
	if (nv) {
		bmk_memset(nv, UNMAGIC2, size2);
		return nv;
	}

	return size2 ? v : NULL;
}

/* XXX: no prototype */
void bmk_memalloc_test(void);
void
bmk_memalloc_test(void)
{
	unsigned long min = TEST_LARGE_MINALLOC;
	unsigned long max = TEST_LARGE_MAXALLOC;
	void **rings; /* yay! */
	void **ring_alloc, **ring_free; /* yay! */
	int i, n;

	randstate = (unsigned)bmk_platform_cpu_clock_epochoffset();

	rings = bmk_memalloc(NALLOC * NRING * sizeof(void *),
	    0, BMK_MEMWHO_USER);
	/* so we can free() immediately without stress */
	bmk_memset(rings, 0, NALLOC * NRING * sizeof(void *));

	for (n = 0;; n = (n+1) % NRING) {
		if (n == 0) {
			bmk_memalloc_printstats();
			if (max == TEST_SMALL_MAXALLOC) {
				min = TEST_LARGE_MINALLOC;
				max = TEST_LARGE_MAXALLOC;
			} else {
				min = TEST_SMALL_MINALLOC;
				max = TEST_SMALL_MAXALLOC;
			}
		}

		ring_alloc = &rings[n * NALLOC];
		ring_free = &rings[((n + NRING/2) % NRING) * NALLOC];
		for (i = 0; i < NALLOC; i++) {
			ring_alloc[i] = testalloc(min, max);
			bmk_memfree(ring_free[i], BMK_MEMWHO_USER);
		}
	}
}
#endif /* MEMALLOC_TESTING */
