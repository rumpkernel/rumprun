/*
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

#include <bmk-pcpu/pcpu.h>

/*
 * The overhead on a block is at least 4 bytes.  When free, this space
 * contains a pointer to the next free block, and the bottom two bits must
 * be zero.  When in use, the first byte is set to MAGIC, and the second
 * byte is the size index.  The remaining bytes are for alignment.
 * If range checking is enabled then a second word holds the size of the
 * requested block, less 1, rounded up to a multiple of sizeof(RMAGIC).
 * The order of elements is critical: ov_magic must overlay the low order
 * bits of ov_next, and ov_magic can not be a valid ov_next bit pattern.
 */
union	overhead {
	union	overhead *ov_next;	/* when free */
	struct {
		unsigned long	ovu_alignpad;	/* padding for alignment */
		unsigned char	ovu_magic;	/* magic number */
		unsigned char	ovu_index;	/* bucket # */

		/* this will be put under RCHECK later */
		unsigned short	ovu_who;	/* who allocated */

#ifdef RCHECK
		unsigned short	ovu_rmagic;	/* range magic number */
		unsigned long	ovu_size;	/* actual block size */
#endif
	} ovu;
#define	ov_alignpad	ovu.ovu_alignpad
#define	ov_magic	ovu.ovu_magic
#define	ov_index	ovu.ovu_index
#define	ov_rmagic	ovu.ovu_rmagic
#define	ov_size		ovu.ovu_size
#define	ov_who		ovu.ovu_who
};

#define	MAGIC		0xef		/* magic # on accounting info */
#define UNMAGIC		0x12		/* magic # != MAGIC */
#define UNMAGIC2	0x24		/* magic # != MAGIC/UNMAGIC */
#ifdef RCHECK
#define RMAGIC		0x5555		/* magic # on range info */
#endif

#ifdef RCHECK
#define	RSLOP		sizeof (unsigned short)
#else
#define	RSLOP		0
#endif

/*
 * nextf[i] is the pointer to the next free block of size 2^(i+MINSHIFT).  The
 * smallest allocatable block is 1<<MINSHIFT bytes.  The overhead information
 * precedes the data area returned to the user.
 */
#define MINSHIFT 5
#define	NBUCKETS 30
#define MINALIGN 16
static	union overhead *nextf[NBUCKETS];

static	unsigned long pagesz;		/* page size */
static	int pagebucket;			/* page size bucket */

/*
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
 */
static unsigned nmalloc[NBUCKETS];

/* not multicore */
#define malloc_lock()
#define malloc_unlock()

static void morecore(int);

void
bmk_memalloc_init(void)
{
	unsigned amt;
	int bucket;

	pagesz = BMK_PCPU_PAGE_SIZE;
	bmk_assert(pagesz > 0);

	bucket = 0;
	amt = 1<<MINSHIFT;
	while (pagesz > amt) {
		amt <<= 1;
		bucket++;
	}
	pagebucket = bucket;
}

void *
bmk_memalloc(unsigned long nbytes, unsigned long align, enum bmk_memwho who)
{
  	union overhead *op;
	void *rv;
	unsigned long allocbytes;
	int bucket;
	unsigned amt;
	unsigned long alignpad;

	if (align & (align-1))
		return NULL;
	if (align < MINALIGN)
		align = MINALIGN;
	
	/* need at least this many bytes plus header to satisfy alignment */
	allocbytes = nbytes + ((sizeof(*op) + (align-1)) & ~(align-1));

	/*
	 * Convert amount of memory requested into closest block size
	 * stored in hash buckets which satisfies request.
	 * Account for space used per block for accounting.
	 */
	if (allocbytes <= pagesz - RSLOP) {
#ifndef RCHECK
		amt = 1<<MINSHIFT;	/* size of first bucket */
		bucket = 0;
#else
		amt = 1<<(MINSHIFT+1);	/* size of first bucket */
		bucket = 1;
#endif
	} else {
		amt = (unsigned)pagesz;
		bucket = pagebucket;
	}
	while (allocbytes > amt) {
		amt <<= 1;
		if (amt == 0)
			return (NULL);
		bucket++;
	}

	malloc_lock();

	/*
	 * If nothing in hash bucket right now,
	 * request more memory from the system.
	 */
  	if ((op = nextf[bucket]) == NULL) {
  		morecore(bucket);
  		if ((op = nextf[bucket]) == NULL) {
			malloc_unlock();
  			return (NULL);
		}
	}
	/* remove from linked list */
  	nextf[bucket] = op->ov_next;

	/* align op before returned memory */
	rv = (void *)(((unsigned long)(op+1) + align - 1) & ~(align - 1));
	alignpad = (unsigned long)rv - (unsigned long)op;

#ifdef MEMALLOC_TESTING
	bmk_memset(op, MAGIC, alignpad);
#endif

	op = ((union overhead *)rv)-1;
	op->ov_magic = MAGIC;
	op->ov_index = bucket;
	op->ov_alignpad = alignpad;
	op->ov_who = who;

  	nmalloc[bucket]++;

	malloc_unlock();
#ifdef RCHECK
	/*
	 * Record allocated size of block and
	 * bound space with magic numbers.
	 */
	op->ov_size = (nbytes + RSLOP - 1) & ~(RSLOP - 1);
	op->ov_rmagic = RMAGIC;
  	*(unsigned short *)((char *)(op + 1) + op->ov_size) = RMAGIC;
#endif

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

/*
 * Allocate more memory to the indicated bucket.
 */
static void
morecore(int bucket)
{
  	union overhead *op;
	long sz;		/* size of desired block */
  	long amt;			/* amount to allocate */
  	long nblks;			/* how many blocks we get */

	if (bucket < pagebucket) {
		amt = 0;
  		nblks = pagesz / (1<<(bucket + MINSHIFT));
		sz = pagesz / nblks;
	} else {
		amt = bucket - pagebucket;
		nblks = 1;
		sz = 0; /* dummy */
	}

	op = (void *)bmk_pgalloc(amt);
	/* no more room! */
  	if (op == NULL)
  		return;

	/*
	 * Add new memory allocated to that on
	 * free list for this hash bucket.
	 */
  	nextf[bucket] = op;
  	while (--nblks > 0) {
		op->ov_next = (union overhead *)
		    (void *)((char *)(void *)op+(unsigned long)sz);
		op = op->ov_next;
  	}
	op->ov_next = NULL;
}

void
bmk_memfree(void *cp, enum bmk_memwho who)
{   
	long size;
	union overhead *op;
	unsigned long alignpad;
	void *origp;

  	if (cp == NULL)
  		return;
	op = ((union overhead *)cp)-1;
	if (op->ov_magic != MAGIC) {
#ifdef MEMALLOC_TESTING
		bmk_assert(0);
#else
		bmk_printf("bmk_memfree: invalid pointer %p\n", cp);
		return;
#endif
	}
	if (op->ov_who != who) {
		bmk_printf("bmk_memfree: mismatch %d vs. %d for %p",
		    op->ov_who, who, cp);
		bmk_platform_halt("bmk_memalloc error");
	}

#ifdef RCHECK
	bmk_assert(op->ov_rmagic == RMAGIC);
	bmk_assert(*(unsigned short *)((char *)(op+1) + op->ov_size) == RMAGIC);
#endif
  	size = op->ov_index;
	alignpad = op->ov_alignpad;
	bmk_assert(size < NBUCKETS);

	malloc_lock();
	origp = (unsigned char *)cp - alignpad;

#ifdef MEMALLOC_TESTING
	{
		unsigned long i;

		for (i = 0;
		    (unsigned char *)origp + i < (unsigned char *)op;
		    i++) {
			bmk_assert(*((unsigned char *)origp + i) == MAGIC);
				
		}
	}
#endif

	op = (void *)origp;
	op->ov_next = nextf[(unsigned int)size];/* also clobbers ov_magic */
  	nextf[(unsigned int)size] = op;

  	nmalloc[(unsigned long)size]--;

	malloc_unlock();
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
	union overhead *op;
  	unsigned long size;
	unsigned long alignpad;
	void *np;

	if (cp == NULL)
		return bmk_memalloc(nbytes, MINALIGN, BMK_MEMWHO_USER);

	if (nbytes == 0) {
		bmk_memfree(cp, BMK_MEMWHO_USER);
		return NULL;
	}

	op = ((union overhead *)cp)-1;
  	size = op->ov_index;
	alignpad = op->ov_alignpad;

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
	union overhead *p;
	unsigned long totfree = 0, totused = 0;
	int i, j;

	bmk_printf("Memory allocation statistics\nfree:\t");
	for (i = 0; i < NBUCKETS; i++) {
		for (j = 0, p = nextf[i]; p; p = p->ov_next, j++)
  			;
		bmk_printf(" %d", j);
		totfree += j * (1 << (i + 3));
  	}
	bmk_printf("\nused:\t");
	for (i = 0; i < NBUCKETS; i++) {
		bmk_printf(" %d", nmalloc[i]);
  		totused += nmalloc[i] * (1 << (i + 3));
  	}
	bmk_printf("\n\tTotal in use: %lukB, total free in buckets: %lukB\n",
	    totused/1024, totfree/1024);
}


/*
 * The rest of this file contains unit tests which run in userspace.
 */

#ifdef MEMALLOC_TESTING

#define TEST_MINALLOC 0
#define TEST_MAXALLOC 64*1024

#define TEST_MINALIGN 1
#define TEST_MAXALIGN 16

#define NALLOC 1024
#define NRING 16

static unsigned randstate;

static int
myrand(void)
{

	return (randstate = randstate * 1103515245 + 12345) % (0x80000000L);
}

static void *
testalloc(void)
{
	void *v, *nv;
	unsigned long size1, size2, align;

	/* doesn't give an even bucket distribution, but ... */
	size1 = myrand() % ((TEST_MAXALLOC-TEST_MINALLOC)+1) + TEST_MINALLOC;
	align = myrand() % ((TEST_MAXALIGN-TEST_MINALIGN)+1) + TEST_MINALIGN;

	v = bmk_memalloc(size1, 1<<align, BMK_MEMWHO_USER);
	if (!v)
		return NULL;
	bmk_assert(((uintptr_t)v & (align-1)) == 0);
	bmk_memset(v, UNMAGIC, size1);

	size2 = myrand() % ((TEST_MAXALLOC-TEST_MINALLOC)+1) + TEST_MINALLOC;
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
	void **rings; /* yay! */
	void **ring_alloc, **ring_free; /* yay! */
	int i, n;

	randstate = (unsigned)bmk_platform_cpu_clock_epochoffset();

	rings = bmk_memalloc(NALLOC * NRING * sizeof(void *),
	    0, BMK_MEMWHO_USER);
	/* so we can free() immediately without stress */
	bmk_memset(rings, 0, NALLOC * NRING * sizeof(void *));

	for (n = 0;; n = (n+1) % NRING) {
		if (n == 0)
			bmk_memalloc_printstats();

		ring_alloc = &rings[n * NALLOC];
		ring_free = &rings[((n + NRING/2) % NRING) * NALLOC];
		for (i = 0; i < NALLOC; i++) {
			ring_alloc[i] = testalloc();
			bmk_memfree(ring_free[i], BMK_MEMWHO_USER);
		}
	}
}
#endif /* MEMALLOC_TESTING */
