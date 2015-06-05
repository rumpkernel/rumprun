/*-
 * Copyright (c) 2015 Martin Lucina.  All Rights Reserved.
 * Copyright (c) 2015 Justin Cormack.  All Rights Reserved.
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

#include <err.h>
#include <fcntl.h>
#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rumprun-base/extract.h>

#include "libuntar.c"

extern const char _binary_rootfs_tar_start[] __attribute__((weak));
extern const char _binary_rootfs_tar_end[] __attribute__((weak));
#define TAR_SIZE ((size_t) (_binary_rootfs_tar_end - _binary_rootfs_tar_start))

static int offset = 0;
static size_t size = 0;

static int
memopen(const char *filename, int flags, mode_t mode)
{
	offset = 0;
	size = TAR_SIZE;
	return 0;
}

static int
memclose(int fd)
{
	return 0;
}

static ssize_t
memread(int fd, void *buf, size_t len)
{
	if (offset + len > size)
		len = size - offset;
	if (len > 0)
		memcpy(buf, _binary_rootfs_tar_start + offset, len);
	offset += len;
	return len;
}

tartype_t memtype = {
	(openfunc_t)memopen,
	(closefunc_t)memclose,
	(readfunc_t)memread
};

int
_rumprun_extract(void)
{
	TAR *t;
	int options = 0;

	if (TAR_SIZE == 0)
		return 0;

	warnx("extracting %zd bytes of baked-in rootfs to /", TAR_SIZE);

	if (tar_open(&t, NULL, &memtype,
		     O_RDONLY, 0, options) == -1)
	{
		warn("tar_open()");
		return -1;
	}

	if (tar_extract_all(t, NULL) != 0)
	{
		warn("tar_extract_all()");
		tar_close(t);
		return -1;
	}

	if (tar_close(t) != 0)
	{
		warn("tar_close()");
		return -1;
	}

	return 0;
}
