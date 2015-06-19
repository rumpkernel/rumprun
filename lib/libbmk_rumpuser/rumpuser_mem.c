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

#include <bmk-core/core.h>
#include <bmk-core/errno.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/pgalloc.h>
#include <bmk-core/null.h>

#include <bmk-rumpuser/core_types.h>
#include <bmk-rumpuser/rumpuser.h>

int
rumpuser_malloc(size_t len, int alignment, void **retval)
{

	/*
	 * If we are allocating precisely a page-sized chunk
	 * (the common case), use the underlying page allocator directly.
	 * This avoids the malloc header overhead for this very
	 * common allocation, leading to 50% better memory use.
	 * We can't easily use the page allocator for larger chucks
	 * of memory, since those allocations might have stricter
	 * alignment restrictions, and therefore it's just
	 * easier to use memalloc() in those rare cases; it's not
	 * as wasteful for larger chunks anyway.
	 *
	 * XXX: how to make sure that rump kernel's and our
	 * page sizes are the same?  Could be problematic especially
	 * for architectures which support multiple page sizes.
	 * Note that the code will continue to work, but the optimization
	 * will not trigger for the common case.
	 */
	if (len == bmk_pagesize) {
		bmk_assert((unsigned long)alignment <= bmk_pagesize);
		*retval = (void *)bmk_pgalloc_one();
	} else {
		*retval = bmk_memalloc(len, alignment, BMK_MEMWHO_RUMPKERN);
	}
	if (*retval)
		return 0;
	else
		return BMK_ENOMEM;
}

void
rumpuser_free(void *buf, size_t buflen)
{

	if (buflen == bmk_pagesize)
		bmk_pgfree_one(buf);
	else
		bmk_memfree(buf, BMK_MEMWHO_RUMPKERN);
}
