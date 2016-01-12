/*-
 * Copyright (c) 2016 Antti Kantee.  All Rights Reserved.
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

#include <sys/param.h>
#include <sys/syscall.h>

#include "rump_private.h"

extern sy_call_t sys_mmap;
extern sy_call_t sys_munmap;
extern sy_call_t sys___msync13;
extern sy_call_t sys_mincore;
extern sy_call_t sys_madvise;
extern sy_call_t sys_mprotect;
extern sy_call_t sys_mlock;
extern sy_call_t sys_mlockall;
extern sy_call_t sys_munlock;
extern sy_call_t sys_munlockall;

#define ENTRY(name) { SYS_##name, sys_##name },
static const struct rump_onesyscall mysys[] = {
	ENTRY(mmap)
	ENTRY(munmap)
	ENTRY(__msync13)
	ENTRY(mincore)
	ENTRY(mprotect)
	ENTRY(mlock)
	ENTRY(mlockall)
	ENTRY(munlock)
	ENTRY(munlockall)
};
#undef ENTRY

RUMP_COMPONENT(RUMP_COMPONENT_SYSCALL)
{

	rump_syscall_boot_establish(mysys, __arraycount(mysys));
}
