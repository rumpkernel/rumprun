/*-
 * Copyright (c) 2014 Antti Kantee.  All Rights Reserved.
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

/*
 * Stub implementations for various rump kernel hypercalls that
 * are not supported on these platforms.  This file should go away
 * with the next revision of the rump kernel hypercall interface.
 */

#include <bmk-core/errno.h>

int rumpuser_stub_nothing(void);
int rumpuser_stub_nothing(void) {return 0;}
#define NOTHING(name) \
int name(void) __attribute__((alias("rumpuser_stub_nothing")));

int rumpuser_stub_enosys(void);
int rumpuser_stub_enosys(void) {return BMK_ENOSYS;}
#define NOSYS(name) \
int name(void) __attribute__((alias("rumpuser_stub_enosys")));

NOTHING(rumpuser_dl_bootstrap);

NOSYS(rumpuser_anonmmap);
NOSYS(rumpuser_unmap);

NOSYS(rumpuser_kill);

NOSYS(rumpuser_daemonize_begin);
NOSYS(rumpuser_daemonize_done);

NOSYS(rumpuser_iovread);
NOSYS(rumpuser_iovwrite);
