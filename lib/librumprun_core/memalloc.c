/*
 * Copyright (c) 2013 Antti Kantee.  All rights reserved.
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
 * Modified for Xen Mini-OS:
 *  + allocate backing storage with page_alloc() instead of sbrk()
 *  + support alignment
 *  + use ANSI C (hey, there's no rush!)
 */

#ifdef MEMALLOC_TESTING
#define PAGE_SIZE getpagesize()
#define MSTATS

#include <sys/cdefs.h>

#include <sys/types.h>
#if defined(RCHECK)
#include <sys/uio.h>
#endif
#if defined(RCHECK) || defined(MSTATS)
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#else

#include <bmk-core/string.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/bmk_ops.h>

#define NULL (void *)0
#define ASSERT(x)

#endif


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
		unsigned long  ovu_alignpad;	/* padding for alignment */
		unsigned char	ovu_magic;	/* magic number */
		unsigned char	ovu_index;	/* bucket # */
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

static	long pagesz;			/* page size */
static	int pagebucket;			/* page size bucket */

#ifdef MSTATS
/*
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
 */
static	u_int nmalloc[NBUCKETS];
#endif

#if 0
#ifdef _REENT
static	mutex_t malloc_mutex = MUTEX_INITIALIZER;
#endif
#endif

/* not currently reentrant on mini-os */
#define malloc_lock()
#define malloc_unlock()

static void morecore(int);
#ifdef MSTATS
void mstats(const char *);
#endif

#if defined(RCHECK) || defined(MEMALLOC_TESTING)
#define	ASSERT(p)   if (!(p)) botch(__STRING(p))
#include <sys/uio.h>

static void botch(const char *);

/*
 * NOTE: since this may be called while malloc_mutex is locked, stdio must not
 *       be used in this function.
 */
static void
botch(const char *s)
{
	struct iovec iov[3];

	iov[0].iov_base	= "\nassertion botched: ";
	iov[0].iov_len	= 20;
	iov[1].iov_base	= (void *)s;
	iov[1].iov_len	= strlen(s);
	iov[2].iov_base	= "\n";
	iov[2].iov_len	= 1;

	/*
	 * This place deserves a word of warning: a cancellation point will
	 * occur when executing writev(), and we might be still owning
	 * malloc_mutex.  At this point we need to disable cancellation
	 * until `after' abort() because i) establishing a cancellation handler
	 * might, depending on the implementation, result in another malloc()
	 * to be executed, and ii) it is really not desirable to let execution
	 * continue.  `Fix me.'
	 * 
	 * Note that holding mutex_lock during abort() is safe.
	 */

	(void)writev(STDERR_FILENO, iov, 3);
	abort();
}
#endif

void *
bmk_memalloc(unsigned long nbytes, unsigned long align)
{
  	union overhead *op;
	void *rv;
	unsigned long allocbytes;
	int bucket;
	unsigned amt;
	unsigned long alignpad;

	malloc_lock();

	if (pagesz == 0) {
		pagesz = bmk_pagesize;
		ASSERT(pagesz > 0);

#if 0
		op = (union overhead *)(void *)sbrk(0);
  		n = n - sizeof (*op) - ((long)op & (n - 1));
		if (n < 0)
			n += pagesz;
		if (n) {
			if (sbrk((int)n) == (void *)-1) {
				malloc_unlock();
				return (NULL);
			}
		}
#endif

		bucket = 0;
		amt = 1<<MINSHIFT;
		while (pagesz > amt) {
			amt <<= 1;
			bucket++;
		}
		pagebucket = bucket;
	}

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
	memset(op, MAGIC, alignpad);
#endif

	op = ((union overhead *)rv)-1;
	op->ov_magic = MAGIC;
	op->ov_index = bucket;
	op->ov_alignpad = alignpad;
#ifdef MSTATS
  	nmalloc[bucket]++;
#endif
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
bmk_xmalloc(unsigned long howmuch)
{
	void *rv;

	rv = bmk_memalloc(howmuch, 0);
#if 0
	if (rv == NULL)
		panic("xmalloc failed");
#endif
	return rv;
}

static void *
corealloc(int shift)
{
	void *v;

#ifdef MEMALLOC_TESTING
	v = malloc((1<<shift) * pagesz);
#else
	v = bmk_ops->bmk_allocpg2(shift);
#endif

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
	op = (void *)corealloc(amt);
	/* no more room! */
  	if (op == NULL)
  		return;
	/*
	 * Add new memory allocated to that on
	 * free list for this hash bucket.
	 */
  	nextf[bucket] = op;
  	while (--nblks > 0) {
		op->ov_next =
		    (union overhead *)(void *)((char *)(void *)op+(unsigned long)sz);
		op = op->ov_next;
  	}
	op->ov_next = NULL;
}

void
bmk_memfree(void *cp)
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
		ASSERT(0);
#endif
		return;				/* sanity */
	}

#ifdef RCHECK
  	ASSERT(op->ov_rmagic == RMAGIC);
	ASSERT(*(unsigned short *)((char *)(op + 1) + op->ov_size) == RMAGIC);
#endif
  	size = op->ov_index;
	alignpad = op->ov_alignpad;
  	ASSERT(size < NBUCKETS);

	malloc_lock();
	origp = (unsigned char *)cp - alignpad;

#ifdef MEMALLOC_TESTING
	{
		unsigned long i;

		for (i = 0;
		    (unsigned char *)origp + i < (unsigned char *)op;
		    i++) {
			ASSERT(*((unsigned char *)origp + i) == MAGIC);
				
		}
	}
#endif

	op = (void *)origp;
	op->ov_next = nextf[(unsigned int)size];/* also clobbers ov_magic */
  	nextf[(unsigned int)size] = op;
#ifdef MSTATS
  	nmalloc[(unsigned long)size]--;
#endif

	malloc_unlock();
}

/*
 * don't do any of "storage compaction" nonsense, "just" the three modes:
 *   + cp == NULL ==> malloc
 *   + nbytes == 0 ==> free
 *   + else ==> realloc
 */
void *
bmk_memrealloc(void *cp, unsigned long nbytes)
{   
	union overhead *op;
  	unsigned long size;
	unsigned long alignpad;
	void *np;

	if (cp == NULL)
		return bmk_memalloc(nbytes, MINALIGN);

	if (nbytes == 0) {
		bmk_memfree(cp);
		return NULL;
	}

	op = ((union overhead *)cp)-1;
  	size = op->ov_index;
	alignpad = op->ov_alignpad;

	/* don't bother "compacting".  don't like it?  don't use realloc! */
	if (((1<<(size+MINSHIFT)) - alignpad) >= nbytes)
		return cp;

	/* we're gonna need a bigger bucket */
	np = bmk_memalloc(nbytes, 8);
	if (np == NULL)
		return NULL;

	bmk_memcpy(np, cp, (1<<(size+MINSHIFT)) - alignpad);
	bmk_memfree(cp);
	return np;
}

#ifdef MSTATS
/*
 * mstats - print out statistics about malloc
 * 
 * Prints two lines of numbers, one showing the length of the free list
 * for each size category, the second showing the number of mallocs -
 * frees for each size category.
 */
void
mstats(const char *s)
{
  	int i, j;
  	union overhead *p;
  	int totfree = 0,
  	totused = 0;

  	fprintf(stderr, "Memory allocation statistics %s\nfree:\t", s);
  	for (i = 0; i < NBUCKETS; i++) {
  		for (j = 0, p = nextf[i]; p; p = p->ov_next, j++)
  			;
  		fprintf(stderr, " %d", j);
  		totfree += j * (1 << (i + 3));
  	}
  	fprintf(stderr, "\nused:\t");
  	for (i = 0; i < NBUCKETS; i++) {
  		fprintf(stderr, " %d", nmalloc[i]);
  		totused += nmalloc[i] * (1 << (i + 3));
  	}
  	fprintf(stderr, "\n\tTotal in use: %d, total free: %d\n",
	    totused, totfree);
}
#endif

#ifdef MEMALLOC_TESTING

#define TEST_MINALLOC 0
#define TEST_MAXALLOC 64*1024

#define TEST_MINALIGN 1
#define TEST_MAXALIGN 16

#define NALLOC 1024
#define NRING 16

static void *
testalloc(void)
{
	void *v, *nv;
	size_t size1, size2, align;

	/* doesn't give an even bucket distribution, but ... */
	size1 = random() % ((TEST_MAXALLOC-TEST_MINALLOC)+1) + TEST_MINALLOC;
	align = random() % ((TEST_MAXALIGN-TEST_MINALIGN)+1) + TEST_MINALIGN;

	v = bmk_memalloc(size1, 1<<align);
	if (!v)
		return NULL;
	ASSERT(((uintptr_t)v & (align-1)) == 0);
	memset(v, UNMAGIC, size1);

	size2 = random() % ((TEST_MAXALLOC-TEST_MINALLOC)+1) + TEST_MINALLOC;
	nv = memrealloc(v, size2);
	if (nv) {
		memset(nv, UNMAGIC2, size2);
		return nv;
	}

	return size2 ? v : NULL;
}

int
main()
{
	void **rings; /* yay! */
	void **ring_alloc, **ring_free; /* yay! */
	int i, n;

	srandom(time(NULL));

	rings = malloc(NALLOC * NRING * sizeof(void *));
	/* so we can free() immediately without stress */
	memset(rings, 0, NALLOC * NRING * sizeof(void *));

	for (n = 0;; n = (n+1) % NRING) {
		if (n == 0)
			mstats("");

		ring_alloc = &rings[n * NALLOC];
		ring_free = &rings[((n + NRING/2) % NRING) * NALLOC];
		for (i = 0; i < NALLOC; i++) {
			ring_alloc[i] = testalloc();
			bmk_memfree(ring_free[i]);
		}
	}
}
#endif
