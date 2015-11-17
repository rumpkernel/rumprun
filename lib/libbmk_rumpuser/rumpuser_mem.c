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

#include <bmk-pcpu/pcpu.h>

#include <bmk-rumpuser/core_types.h>
#include <bmk-rumpuser/rumpuser.h>

static int
len2order(unsigned long len)
{
	int v;

	v = 8*sizeof(len) - __builtin_clzl(len);
	if ((len & (len-1)) == 0)
		v--;

	return v - BMK_PCPU_PAGE_SHIFT;
}

int
rumpuser_malloc(size_t len, int alignment, void **retval)
{

	/*
	 * Allocate large chunks with the page allocator to avoid
	 * malloc overhead.
	 */
	if (len < BMK_PCPU_PAGE_SIZE) {
		*retval = bmk_memalloc(len, alignment, BMK_MEMWHO_RUMPKERN);
	} else {
		bmk_assert((alignment & (alignment-1)) == 0);
		if (alignment < BMK_PCPU_PAGE_SIZE)
			alignment = BMK_PCPU_PAGE_SIZE;
		*retval = bmk_pgalloc_align(len2order(len), alignment);
	}
	if (*retval)
		return 0;
	else
		return BMK_ENOMEM;
}

void
rumpuser_free(void *buf, size_t buflen)
{

	if (buflen < BMK_PCPU_PAGE_SIZE) {
		bmk_memfree(buf, BMK_MEMWHO_RUMPKERN);
	} else {
		bmk_pgfree(buf, len2order(buflen));
	}
}
