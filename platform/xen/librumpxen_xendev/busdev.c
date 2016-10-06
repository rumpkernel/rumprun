/*
 * xenbus_dev.c
 * 
 * Driver giving user-space access to the kernel's xenbus connection
 * to xenstore.  Adapted heavily from NetBSD's xenbus_dev.c, so much
 * so that practically none of the original remains.
 * 
 * Copyright (c) 2014 Citrix
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
 *
 * (From original xenbus_dev.c:)
 * Copyright (c) 2005, Christian Limpach
 * Copyright (c) 2005, Rusty Russell, IBM Corporation
 *
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
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

#include <xen/io/xs_wire.h>

#include "busdev_user.h"

struct rumpxenbus_data_dev {
	kmutex_t lock;

	_Bool want_restart;

	struct rumpxenbus_data_common dc;

	/* Partially read response. */
	struct xsd_sockmsg *rmsg; /* .id==user_id; data follows */
	int rmsg_done;
	void (*rmsg_free)(void*);

	struct selinfo selinfo;
	/* The lock used for the purposes described in select(9)
	 * is xenbus_req_lock, not dd->lock. */
};


void
rumpxenbus_write_trouble(struct rumpxenbus_data_common *dc, const char *what)
{
	printf("xenbus dev: bad write: %s\n", what);

#ifdef RUMP_DEV_XEN_DEBUG
	{
		unsigned int i;
		printf(" %d bytes:", dc->wbuf_used);
		for (i=0; i<dc->wbuf_used; i++) {
			if (!(i & 3)) printf(" ");
			printf("%02x", dc->wbuf.buffer[i]);
		}
		printf(".\n");
	}
#endif /*RUMP_DEV_XEN_DEBUG*/

	dc->wbuf_used = 0; /* discard everything buffered */
}

/*----- request handling (writes to the device) -----*/

#define wbuf      dc.wbuf
#define wbuf_used dc.wbuf_used

static int
xenbus_dev_write(struct file *fp, off_t *offset, struct uio *uio,
		 kauth_cred_t cred, int flags)
{
	struct rumpxenbus_data_dev *const dd = fp->f_data;
	struct rumpxenbus_data_common *const dc = &dd->dc;
	int err;

	DPRINTF(("/dev/xen/xenbus[%p,dd=%p]: write...\n",dc,dd));

	if (uio->uio_offset < 0)
		return EINVAL;

	mutex_enter(&dd->lock);

	for (;;) { /* keep reading more until we're done */

		if (!uio->uio_resid)
			break;

		uio->uio_offset = dd->wbuf_used;
		err = uiomove(dd->wbuf.buffer + dd->wbuf_used,
			      sizeof(dd->wbuf.buffer) -  dd->wbuf_used,
			      uio);
		dd->wbuf_used = uio->uio_offset;
		if (err)
			goto end;

		for (;;) { /* process message(s) in the buffer */

			if (dd->wbuf_used < sizeof(dd->wbuf.msg))
				break;

			if (dd->wbuf.msg.len > XENSTORE_PAYLOAD_MAX)
				WTROUBLE("too much payload in packet");

			uint32_t packetlen =
				dd->wbuf.msg.len + sizeof(dd->wbuf.msg);

			KASSERT(packetlen <= sizeof(dd->wbuf.buffer));

			if (dd->wbuf_used < packetlen)
				break;

			err = rumpxenbus_process_request(&dd->dc);

			if (dd->wbuf_used) {
				/* Remove from the buffer before checking
				 * for errors - but some errors may have
				 * emptied the buffer already. */
				dd->wbuf_used -= packetlen;
				memmove(dd->wbuf.buffer,
					dd->wbuf.buffer + packetlen,
					dd->wbuf_used);
			}

			if (err)
				goto end;
		}
	}

	err = 0;
end:
	mutex_exit(&dd->lock);

	DPRINTF(("/dev/xen/xenbus: write done, err=%d\n", err));
	return err;
}

/*----- response and watch event handling (reads from the device) -----*/

void rumpxenbus_block_before(struct rumpxenbus_data_common *dc)
{
	struct rumpxenbus_data_dev *dd =
		container_of(dc, struct rumpxenbus_data_dev, dc);
	mutex_exit(&dd->lock);
}

void rumpxenbus_block_after(struct rumpxenbus_data_common *dc)
{
	struct rumpxenbus_data_dev *dd =
		container_of(dc, struct rumpxenbus_data_dev, dc);
	mutex_enter(&dd->lock);
}

static int
xenbus_dev_read(struct file *fp, off_t *offset, struct uio *uio,
		kauth_cred_t cred, int flags)
{
	struct rumpxenbus_data_dev *const dd = fp->f_data;
	struct rumpxenbus_data_common *const dc = &dd->dc;
	size_t org_resid = uio->uio_resid;
	int err;

	DPRINTF(("/dev/xen/xenbus[%p,dd=%p:"
		 " read (nonblock=%d)...\n",
		 dc,dd, !(fp->f_flag & FNONBLOCK)));
	mutex_enter(&dd->lock);

	for (;;) {
		DPRINTF(("/dev/xen/xenbus: read... uio_resid=%lu (org=%lu)"
			 " q.enomem=%d\n",
			 (unsigned long)uio->uio_resid,
			 (unsigned long)org_resid,
			 dc->queued_enomem));
		if (dc->queued_enomem) {
			if (org_resid != uio->uio_resid)
				/* return early now; report it next time */
				break;
			err = ENOMEM;
			dc->queued_enomem = 0;
			goto end;
		}

		if (!uio->uio_resid)
			/* done what we have been asked to do */
			break;

		if (!dd->rmsg) {
			int err_if_block = 0;
			if (dd->want_restart) {
				err_if_block = ERESTART;
			} else if (fp->f_flag & FNONBLOCK) {
				err_if_block = EAGAIN;
			}

			dd->rmsg = rumpxenbus_next_event_msg(&dd->dc,
						 !err_if_block,
						 &dd->rmsg_free);
			DPRINTF(("/dev/xen/xenbus: read... rmsg=%p (eib=%d)\n",
				 dd->rmsg, err_if_block));
			if (!dd->rmsg) {
				if (uio->uio_resid != org_resid)
					/* Done something, claim success. */
					break;
				err = err_if_block;
				goto end;
			}
		}

		uint32_t avail = sizeof(*dd->rmsg) + dd->rmsg->len;
		KASSERT(avail < BUFFER_SIZE*2); /* sanity check */
		KASSERT(avail > 0);
		KASSERT(dd->rmsg_done <= avail);

		DPRINTF(("/dev/xen/xenbus: read... rmsg->len=%lu"
			 " msg_done=%lu avail=%lu\n",
			 (unsigned long)dd->rmsg->len,
			 (unsigned long)dd->rmsg_done,
			 (unsigned long)avail));

		uio->uio_offset = dd->rmsg_done;
		err = uiomove((char*)dd->rmsg + dd->rmsg_done,
			      avail - dd->rmsg_done,
			      uio);
		dd->rmsg_done = uio->uio_offset;
		if (err)
			goto end;

		if (dd->rmsg_done == avail) {
			DPRINTF(("/dev/xen/xenbus: read... msg complete\n"));
			dd->rmsg_free(dd->rmsg);
			dd->rmsg = 0;
			dd->rmsg_done = 0;
		}
	}

	err = 0;

end:
	mutex_exit(&dd->lock);
	DPRINTF(("/dev/xen/xenbus: read done, err=%d\n", err));
	return err;
}

/*----- more exciting reading -----*/

#define RBITS (POLLIN  | POLLRDNORM)
#define WBITS (POLLOUT | POLLWRNORM)

void rumpxenbus_dev_xb_wakeup(struct rumpxenbus_data_common *dc)
{
	struct rumpxenbus_data_dev *dd =
		container_of(dc, struct rumpxenbus_data_dev, dc);
	DPRINTF(("/dev/xen/xenbus[%p,dd=%p]: wakeup\n",dd,dc));
	selnotify(&dd->selinfo, RBITS, NOTE_SUBMIT);
}

static void
xenbus_dev_restart(file_t *fp)
{
	struct rumpxenbus_data_dev *dd = fp->f_data;

	DPRINTF(("/dev/xen/xenbus[dd=%p]: restart!\n",dd));

	mutex_enter(&dd->lock);
	dd->want_restart |= 1;
	rumpxenbus_dev_restart_wakeup(&dd->dc);
	mutex_exit(&dd->lock);
}

static int
xenbus_dev_poll(struct file *fp, int events)
{
	struct rumpxenbus_data_dev *const dd = fp->f_data;
	struct rumpxenbus_data_common *const dc = &dd->dc;
	int revents = 0;

	DPRINTF(("/dev/xen/xenbus[%p,dd=%p]: poll events=0%o...\n",
		 dc,dd,events));

	mutex_enter(&dd->lock);

	/* always writeable - we don't do proper blocking for writing
	 * since this can only wait at most until other requests have
	 * been handled by xenstored */
	revents |= events & WBITS;

	if (events & RBITS)
		if (dd->rmsg || dc->queued_enomem || dd->want_restart)
			revents |= events & RBITS;

	if (!revents) {
		if (events & RBITS)
			selrecord(curlwp, &dd->selinfo);
	}

	mutex_exit(&dd->lock);

	DPRINTF(("/dev/xen/xenbus: poll events=0%o done, revents=0%o\n",
		 events, revents));
	return revents;
}

/*----- setup etc. -----*/

static int
xenbus_dev_close(struct file *fp)
{
	struct rumpxenbus_data_dev *dd = fp->f_data;

	DPRINTF(("/dev/xen/xenbus[dd=%p]: close...\n",dd));

	/* Not neeeded against concurrent access (we assume!)
	 * but next_event_msg will want to unlock and relock it */
	mutex_enter(&dd->lock);

	xbd_free(dd->rmsg);
	dd->rmsg = 0;

	rumpxenbus_dev_user_shutdown(&dd->dc);

	KASSERT(!dd->rmsg);

	DPRINTF(("/dev/xen/xenbus: close seldestroy...\n"));
	seldestroy(&dd->selinfo);
	xbd_free(dd);

	DPRINTF(("/dev/xen/xenbus: close done.\n"));
	return 0;
}

const struct fileops xenbus_dev_fileops = {
        .fo_read = xenbus_dev_read,
        .fo_write = xenbus_dev_write,
        .fo_ioctl = fbadop_ioctl,
        .fo_fcntl = fnullop_fcntl,
        .fo_poll = xenbus_dev_poll,
        .fo_stat = fbadop_stat,
        .fo_close = xenbus_dev_close,
        .fo_kqfilter = fnullop_kqfilter,
        .fo_restart = xenbus_dev_restart,
};

int
xenbus_dev_open(struct file *fp, void **fdata_r)
{
	struct rumpxenbus_data_dev *dd;
	int err;

	DPRINTF(("/dev/xen/xenbus: open: entry...\n"));

	dd = xbd_malloc(sizeof(*dd));
	if (!dd)
		return ENOMEM;

	dd->dc.du = 0;

	DPRINTF(("/dev/xen/xenbus[%p,du=%p]: open: alloc...\n",&dd->dc,dd));

	err = rumpxenbus_dev_user_open(&dd->dc);
	if (err) {
		xbd_free(dd);
		return err;
	}

	mutex_init(&dd->lock, MUTEX_DEFAULT, IPL_HIGH);
	dd->want_restart = 0;
	dd->rmsg = 0;
	dd->rmsg_done = 0;
	selinit(&dd->selinfo);

	*fdata_r = dd;

	DPRINTF(("/dev/xen/xenbus[%p,dd=%p,du=%p]: opened.\n",
		 &dd->dc, dd, dd->dc.du));

	return 0;
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
