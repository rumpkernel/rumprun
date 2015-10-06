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

#ifndef _BMK_CORE_CORE_H_
#define _BMK_CORE_CORE_H_

#include <bmk-core/platform.h>

#include <bmk-pcpu/pcpu.h>

int bmk_core_init(unsigned long);

#define bmk_assert(x)							\
  do {									\
	if (__builtin_expect(!(x), 0)) {				\
		bmk_platform_halt("assert \"" #x "\" FAILED\n");	\
	}								\
  } while (0)

/*
 * Compile-time assert.
 *
 * We use it only if cpp supports __COUNTER__ (everything from
 * the past several years does).
 *
 * And hooray for cpp macro expansion.
 */
#ifdef __COUNTER__
#define __bmk_ctassert_c(x,c)	typedef char __ct##c[(x) ? 1 : -1]	\
				    __attribute__((unused))
#define __bmk_ctassert(x,c)	__bmk_ctassert_c(x,c)
#define bmk_ctassert(x)		__bmk_ctassert(x,__COUNTER__)
#else
#define bmk_ctassert(x)
#endif

extern unsigned long bmk_stackpageorder, bmk_stacksize;

void *bmk_mainstackbase;
unsigned long bmk_mainstacksize;

#define bmk_round_page(_p_) \
    (((_p_) + (BMK_PCPU_PAGE_SIZE-1)) & ~(BMK_PCPU_PAGE_SIZE-1))
#define bmk_trunc_page(_p_) \
    ((_p_) & ~(BMK_PCPU_PAGE_SIZE-1))

#endif /* _BMK_CORE_CORE_H_ */
