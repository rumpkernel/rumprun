/*-
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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

#ifndef _BMK_CORE_MEMALLOC_H_
#define _BMK_CORE_MEMALLOC_H_

/*
 * Allocator levels/priorities/somethinglikethat:
 *
 *   BMK_MEMWHO_WIREDBMK:
 *	This level is the most critical one.  Everything runs on top of
 *	it, and usually if a WIREDBMK allocation fails, everything fails.
 *
 *   BMK_MEMWHO_RUMPKERN:
 *	This level is used by "pageable" (and/or non-critical) memory
 *	allocated by the rump kernel hypercalls.  The memory is not
 *	really pageable (no VM!), but since the rump kernel a) attempts
 *	to cache until it has consumed all memory b) will release said
 *	caches if we ask it to, the memory is not "wired" in the same
 *	sense as WIREDBMK is.
 *
 *   BMK_MEMWHO_USER:
 *	This level is used by user-level memory allocation.  User level
 *	memory is "worse" that the RUMPKERN level since generally speaking
 *	applications do not have a way to respond to memory backpressure.
 *
 *
 * The approximate memory flow is (or as of writing this, "will be") the
 * following:
 *   1) WIREDBMK used is predictable and it will be handed a fixed size
 *	chunk of pages.  This pool should never run out of pages.
 *   2) RUMPKERN will try to allocate all available memory, or up to a
 *	dynamically configurable limit, but still at least up to a
 *	hardcoded low watermark (hardcoded = decided a boot time).
 *   3) USER will allocate as much memory as it needs.  In case USER is
 *      out of memory, it will notify the rump kernel to free up some
 *	memory.
 *
 *   TODO: figure out if we want to support the case where USER allocates
 *	and then frees a lot of memory (will require changes to the allocator)
 */

enum bmk_memwho {
	BMK_MEMWHO_WIREDBMK,
	BMK_MEMWHO_RUMPKERN,
	BMK_MEMWHO_USER
};

void	bmk_memalloc_init(void);

void *  bmk_memalloc(unsigned long, unsigned long, enum bmk_memwho);
void *  bmk_memcalloc(unsigned long, unsigned long, enum bmk_memwho);
void    bmk_memfree(void *, enum bmk_memwho);

void *  bmk_memrealloc_user(void *, unsigned long);

void *  bmk_xmalloc_bmk(unsigned long);

/* diagnostic */
void	bmk_memalloc_printstats(void);

#endif /* _BMK_CORE_MEMALLOC_H_ */
