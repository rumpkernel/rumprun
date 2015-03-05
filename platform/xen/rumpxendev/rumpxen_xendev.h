/*
 * Copyright (c) 2014 Citrix
 * 
 * Header for /dev/xen* in a rumpkernel.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef RUMP_DEV_XEN_H
#define RUMP_DEV_XEN_H

#include <sys/filedesc.h>
#include <sys/malloc.h>
#include <sys/lwp.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/file.h>
#include <sys/poll.h>


/* nicked from NetBSD sys/dev/pci/cxgb/cxgb_adapter.h */
#define container_of(p, stype, field) ((stype *)(((uint8_t *)(p)) - offsetof(stype, field)))

//#define RUMP_DEV_XEN_DEBUG 1

#ifdef RUMP_DEV_XEN_DEBUG
#define DPRINTF(a) (printf a)
#else
#define DPRINTF(a) /* nothing */
#endif


/* Device operations, for devs table in rump_dev_xen.c */

extern int xenbus_dev_open(struct file *fp, void **fdata);
extern const struct fileops xenbus_dev_fileops;


static inline void*
xbd_malloc(size_t sz)
{
	return malloc(sz, M_DEVBUF, M_WAITOK);
}

static inline void
xbd_free(void *p)
{
	if (p) /* free(9) is not like free(3)! */
		free(p, M_DEVBUF);
}

char *xbd_strdup(const char *s);

#endif /*RUMP_DEV_XEN_H*/

/*
 * Local variables:
 *  c-file-style: "linux"
 *  indent-tabs-mode: t
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
