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

#ifndef _BMK_CORE_ERRNO_H_
#define _BMK_CORE_ERRNO_H_

/*
 * errno values.
 * these "accidentally" match NetBSD ones for convenience reasons.
 *
 * NOTE: we don't slurp in the whole errno table for a reason.
 * Please be critical when adding new values!  Prefer to remove them!
 */

#define BMK_ENOENT		2
#define BMK_EIO			5
#define BMK_ENXIO		6
#define BMK_E2BIG		7
#define BMK_EBADF		9
#define BMK_ENOMEM		12
#define BMK_EBUSY		16
#define BMK_EINVAL		22
#define BMK_EROFS		30
#define BMK_ETIMEDOUT		60
#define BMK_ENOSYS		78

#define BMK_EGENERIC		BMK_EINVAL

#endif /* _BMK_CORE_ERRNO_H_ */
