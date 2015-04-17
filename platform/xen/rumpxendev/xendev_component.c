/*
 * rump_dev_xen.c
 * 
 * Machinery for setting up the contents of /dev/xen* in a rumpkernel.
 * 
 * Copyright (c) 2014 Citrix
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: $");

#include "rumpxen_xendev.h"

#include "rump_private.h"
#include "rump_vfs_private.h"

#include <sys/vfs_syscalls.h>

char *xbd_strdup(const char *s)
{
	char *r;
	size_t l = strlen(s) + 1;
	r = xbd_malloc(l);
	if (!r)
		return r;
	memcpy(r, s, l);
	return r;
}

#define DEV_XEN "/dev/xen"

static const struct xen_dev_info {
	const char *path;
	int (*xd_open)(struct file *fp, void **fdata_r);
	const struct fileops *fo;
} devs[] = {
#define XDEV(cmin, path, component)					\
	[cmin] = { path, component##_dev_open, &component##_dev_fileops }
	XDEV(0, DEV_XEN "/xenbus", xenbus),
#undef XDEV
};

#define NUM_DEV_INFOS (sizeof(devs)/sizeof(devs[0]))

static int
xen_dev_open(dev_t dev, int flags, int mode, struct lwp *l)
{
	const struct xen_dev_info *xdinfo;
	int fd, err;
	struct file *fp;
	void *fdata;

	DPRINTF(("xen devsw: opening minor=%lu\n", (unsigned long)minor(dev)));

        if (minor(dev) < 0 || minor(dev) >= NUM_DEV_INFOS)
		return ENODEV;

	xdinfo = &devs[minor(dev)];

	if (!xdinfo->xd_open)
		return ENODEV;

	err = fd_allocfile(&fp, &fd);
	if (err)
		return err;

	DPRINTF(("%s: opening...\n", xdinfo->path));

	err = xdinfo->xd_open(fp, &fdata);
	if (err) {
		fd_abort(curproc, fp, fd);
		return err;
	}

	DPRINTF(("%s: opened, fd_clone\n", xdinfo->path));

	return fd_clone(fp, fd, flags, xdinfo->fo, fdata);
}

static const struct cdevsw xen_dev_cdevsw = {
	.d_open = xen_dev_open,
	.d_close = nullclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_flag = D_OTHER
};

RUMP_COMPONENT(RUMP_COMPONENT_DEV)
{
        devmajor_t bmaj, cmaj;
	devminor_t cmin;
        int err;
	const struct xen_dev_info *xdinfo;

	DPRINTF(("xen devsw: attaching\n"));

	err = do_sys_mkdir(DEV_XEN, 0755, UIO_SYSSPACE);
	if (err && err != EEXIST)
		panic("xen devsw: mkdir " DEV_XEN " failed: %d", err);

        bmaj = cmaj = NODEVMAJOR;
        err = devsw_attach("xen", NULL, &bmaj, &xen_dev_cdevsw, &cmaj);
	if (err)
                panic("xen devsw: attach failed: %d", err);

	for (cmin = 0; cmin < NUM_DEV_INFOS; cmin++) {
	     xdinfo = &devs[cmin];
	     err = rump_vfs_makeonedevnode(S_IFCHR, xdinfo->path, cmaj, cmin);
	     if (err)
		     panic("%s: cannot create device node: %d",
			   xdinfo->path, err);
	     DPRINTF(("%s: created, %lu.%lu\n",
		      xdinfo->path, (unsigned long)cmaj, (unsigned long)cmin));
	}
}

/*
 * Local variables:
 *  c-file-style: "linux"
 *  indent-tabs-mode: t
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
 
