/*
 * evtdev.c
 *
 * Driver giving user-space access to the kernel's event channel.
 *
 * Copyright (c) 2015 Wei Liu <wei.liu2@citrix.com>
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

#include <sys/cdefs.h>
#include <sys/atomic.h>
#include <sys/kmem.h>
#include <sys/resource.h>
#include <sys/vnode.h>

#include "rumpxen_xendev.h"

#include <bmk-rumpuser/rumpuser.h>
#include <bmk-core/memalloc.h>

#include <mini-os/events.h>
#include <mini-os/wait.h>

/* For ioctl interface. */
#include "xenio3.h"

/*----- data structures -----*/
#define EVTDEV_RING_SIZE 2048
#define EVTDEV_RING_MASK 2047
#define BYTES_PER_PORT   4

/* See the rump_evtdev_callback for locking information */
u_int xenevt_ring[EVTDEV_RING_SIZE];
u_int xenevt_ring_prod, xenevt_ring_cons;

struct xenevt_dev_data {
	u_int ring[EVTDEV_RING_SIZE];
	u_int ring_cons;
	u_int ring_prod;
#define EVTDEV_F_OVERFLOW 0x1 	/* ring overflow */
	u_int flags;

	kmutex_t lock;
	kcondvar_t cv;
	struct selinfo selinfo; /* used by poll, see select(9) */
};

/* Kernel event -> device instance mapping */
static kmutex_t devevent_lock;
static struct xenevt_dev_data *devevents[NR_EVENT_CHANNELS];

/*----- helpers -----*/
#define WBITS (POLLOUT | POLLWRNORM)
#define RBITS (POLLIN  | POLLRDNORM)

/* call with d->lock held */
static void queue(struct xenevt_dev_data *d, u_int port)
{
	KASSERT(mutex_owned(&d->lock));

	if (d->ring_cons == ((d->ring_prod + 1) & EVTDEV_RING_MASK)) {
		d->flags |= EVTDEV_F_OVERFLOW;
		printf("evtdev: ring overflow port %d\n", port);
	} else {
		d->ring[d->ring_prod] = port;
		membar_producer();
		d->ring_prod = (d->ring_prod + 1) & EVTDEV_RING_MASK;
	}
	/* notify */
	cv_signal(&d->cv);
	selnotify(&d->selinfo, RBITS, NOTE_SUBMIT);
}

/* This callback is serialised by mini-os */
static void rump_evtdev_callback(u_int port)
{
	if (xenevt_ring_cons == ((xenevt_ring_prod + 1) & EVTDEV_RING_MASK)) {
		printf("xenevt driver ring overflowed!\n");
	} else {
		xenevt_ring[xenevt_ring_prod] = port;
		membar_producer();
		xenevt_ring_prod = (xenevt_ring_prod + 1) & EVTDEV_RING_MASK;
	}

	minios_wake_up(&minios_events_waitq);
}

static void xenevt_thread_func(void *ign)
{
	u_int prod = xenevt_ring_prod;
	u_int cons;

	/* give us a rump kernel context */
	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_lwproc_newlwp(0);
	rumpuser__hyp.hyp_unschedule();

	for (;;) {
		minios_wait_event(minios_events_waitq, xenevt_ring_prod != prod);
		prod = xenevt_ring_prod;
		cons = xenevt_ring_cons;

		membar_sync();

		while (cons != prod) {
			u_int port = xenevt_ring[cons];
			struct xenevt_dev_data *d;

			KASSERT(port < NR_EVENT_CHANNELS);

			mutex_enter(&devevent_lock);

			d = devevents[port];

			KASSERT(d);

			mutex_enter(&d->lock);

			queue(d, port);

			mutex_exit(&d->lock);
			mutex_exit(&devevent_lock);

			cons++;
		}

		membar_sync();

		xenevt_ring_cons = cons;
	}
}

/*----- request handling (writes to the device) -----*/
static int
xenevt_dev_write(struct file *fp, off_t *offset, struct uio *uio,
		 kauth_cred_t cred, int flags)
{
	struct xenevt_dev_data *d = fp->f_data;
	uint16_t *chans = NULL;
	int i, nentries, err;
	size_t size = 0;

	DPRINTF(("/dev/xenevt: write...\n"));

	if (uio->uio_resid == 0) {
		err = 0;
		goto out;
	}

	nentries = uio->uio_resid / sizeof(uint16_t);
	if (nentries > NR_EVENT_CHANNELS) {
		err = EMSGSIZE;
		goto out;
	}

	size = nentries * sizeof(uint16_t);
	chans = kmem_alloc(size, KM_SLEEP);

	err = uiomove(chans, uio->uio_resid, uio);
	if (err) goto out;

	mutex_enter(&devevent_lock);
	for (i = 0; i < nentries; i++) {
		if (chans[i] < NR_EVENT_CHANNELS &&
		    devevents[chans[i]] == d)
			minios_unmask_evtchn(chans[i]);
	}
	mutex_exit(&devevent_lock);

	KASSERT(err == 0);
out:
	DPRINTF(("/dev/xenevt: write done, err=%d\n", err));
	if (size) kmem_free(chans, size);
	return err;
}

static int
xenevt_dev_read(struct file *fp, off_t *offset, struct uio *uio,
		kauth_cred_t cred, int read_flags)
{
	struct xenevt_dev_data *d = fp->f_data;
	u_int cons, prod, len, uio_len;
	int err;

	DPRINTF(("/dev/xenevt: read...\n"));

	mutex_enter(&d->lock);

	err = 0;
	while (err == 0) {
		cons = d->ring_cons;
		prod = d->ring_prod;

		if (cons != prod) break; /* data available */

		if (d->flags & EVTDEV_F_OVERFLOW) break;

		/* nothing to read */
		if ((fp->f_flag & FNONBLOCK) == 0)
			err = cv_wait_sig(&d->cv, &d->lock);
		else
			err = EAGAIN;
	}

	if (err == 0 && (d->flags & EVTDEV_F_OVERFLOW))
		err = EFBIG;

	if (err) goto out;

	uio_len = uio->uio_resid / BYTES_PER_PORT;
	if (cons <= prod)
		len = prod - cons;
	else
		len = EVTDEV_RING_SIZE - cons;
	if (len > uio_len)
		len = uio_len;
	err = uiomove(&d->ring[cons], len * BYTES_PER_PORT, uio);
	if (err) goto out;

	cons = (cons + len) & EVTDEV_RING_MASK;
	uio_len = uio->uio_resid / BYTES_PER_PORT;
	if (uio_len == 0) goto done;

	/* ring wrapped */
	len = prod - cons;
	if (len > uio_len)
		len = uio_len;
	err = uiomove(&d->ring[cons], len * BYTES_PER_PORT, uio);
	if (err) goto out;
	cons = (cons + len) & EVTDEV_RING_MASK;

done:
	d->ring_cons = cons;
out:
	mutex_exit(&d->lock);
	DPRINTF(("/dev/xenevt: read done, err=%d\n", err));
	return err;
}

/*----- more exciting reading -----*/
static int
xenevt_dev_poll(struct file *fp, int events)
{
	struct xenevt_dev_data *d = fp->f_data;
	int revents = 0;

	DPRINTF(("/dev/xenevt: poll events=0x%x...\n", events));

	mutex_enter(&d->lock);

	/* always writable because write is used to unmask event
	 * channel */
	revents |= events & WBITS;

	if ((events & RBITS) && (d->ring_prod != d->ring_cons))
		revents |= events & RBITS;

	/* in the case caller only interests in read but no data
	 * available to read */
	if (!revents && (events & RBITS))
		selrecord(curlwp, &d->selinfo);

	mutex_exit(&d->lock);
	DPRINTF(("/dev/xenevt: poll events=0x%x done, revents=0x%x\n",
		 events, revents));
	return revents;
}

static int
xenevt_dev_ioctl(struct file *fp, ulong cmd, void *data)
{
	struct xenevt_dev_data *d = fp->f_data;
	int err;

	switch (cmd) {
	case IOCTL_EVTCHN_RESET:
	{
		mutex_enter(&d->lock);
		d->ring_cons = d->ring_prod = 0;
		d->flags = 0;
		mutex_exit(&d->lock);
		break;
	}
	case IOCTL_EVTCHN_BIND_VIRQ:
	{
		struct ioctl_evtchn_bind_virq *bind_virq = data;
		evtchn_bind_virq_t op;

		op.virq = bind_virq->virq;
		op.vcpu = 0;
		if ((err = minios_event_channel_op(EVTCHNOP_bind_virq, &op))) {
			printf("IOCTL_EVTCHN_BIND_VIRQ failed: virq %d error %d\n",
			       bind_virq->virq, err);
			return -err;
		}
		bind_virq->port = op.port;
		mutex_enter(&devevent_lock);
		KASSERT(devevents[bind_virq->port] == NULL);
		devevents[bind_virq->port] = d;
		mutex_exit(&devevent_lock);
		minios_bind_evtchn(bind_virq->port, minios_evtdev_handler, d);
		minios_unmask_evtchn(bind_virq->port);

		break;
	}
	case IOCTL_EVTCHN_BIND_INTERDOMAIN:
	{
		struct ioctl_evtchn_bind_interdomain *bind_intd = data;
		evtchn_bind_interdomain_t op;

		op.remote_dom = bind_intd->remote_domain;
		op.remote_port = bind_intd->remote_port;
		if ((err = minios_event_channel_op(EVTCHNOP_bind_interdomain, &op))) {
			printf("IOCTL_EVTCHN_BIND_INTERDOMAIN failed: "
			       "remote domain %d port %d error %d\n",
			       bind_intd->remote_domain, bind_intd->remote_port, err);
			return -err;
		}
		bind_intd->port = op.local_port;
		mutex_enter(&devevent_lock);
		KASSERT(devevents[bind_intd->port] == NULL);
		devevents[bind_intd->port] = d;
		mutex_exit(&devevent_lock);
		minios_bind_evtchn(bind_intd->port, minios_evtdev_handler, d);
		minios_unmask_evtchn(bind_intd->port);

		break;
	}
	case IOCTL_EVTCHN_BIND_UNBOUND_PORT:
	{
		struct ioctl_evtchn_bind_unbound_port *bind_unbound = data;
		evtchn_alloc_unbound_t op;

		op.dom = DOMID_SELF;
		op.remote_dom = bind_unbound->remote_domain;
		if ((err = minios_event_channel_op(EVTCHNOP_alloc_unbound, &op))) {
			printf("IOCTL_EVTCHN_BIND_UNBOUND_PORT failed: "
			       "remote domain %d error %d\n",
			       bind_unbound->remote_domain, err);
			return -err;
		}
		bind_unbound->port = op.port;
		mutex_enter(&devevent_lock);
		KASSERT(devevents[bind_unbound->port] == NULL);
		devevents[bind_unbound->port] = d;
		mutex_exit(&devevent_lock);
		minios_bind_evtchn(bind_unbound->port, minios_evtdev_handler, d);
		minios_unmask_evtchn(bind_unbound->port);

		break;
	}
	case IOCTL_EVTCHN_UNBIND:
	{
		struct ioctl_evtchn_unbind *unbind = data;

		if (unbind->port >= NR_EVENT_CHANNELS)
			return EINVAL;
		mutex_enter(&devevent_lock);
		if (devevents[unbind->port] != d) {
			mutex_exit(&devevent_lock);
			return ENOTCONN;
		}
		devevents[unbind->port] = NULL;
		mutex_exit(&devevent_lock);
		minios_mask_evtchn(unbind->port);
		minios_unbind_evtchn(unbind->port);

		break;
	}
	case IOCTL_EVTCHN_NOTIFY:
	{
		struct ioctl_evtchn_notify *notify = data;

		if (notify->port >= NR_EVENT_CHANNELS)
			return EINVAL;
		mutex_enter(&devevent_lock);
		if (devevents[notify->port] != d) {
			mutex_exit(&devevent_lock);
			return ENOTCONN;
		}
		minios_notify_remote_via_evtchn(notify->port);
		mutex_exit(&devevent_lock);

		break;
	}
	default:
		return EINVAL;
	}

	return 0;
}

/*----- setup etc. -----*/

static int
xenevt_dev_close(struct file *fp)
{
	struct xenevt_dev_data *d = fp->f_data;
	int i;

	DPRINTF(("/dev/xenevt: close...\n"));

	mutex_enter(&devevent_lock);
	mutex_enter(&d->lock);
	for (i = 0; i < NR_EVENT_CHANNELS; i++) {
		if (devevents[i] == d) {
			minios_unbind_evtchn(i);
			devevents[i] = NULL;
		}
	}
	mutex_exit(&d->lock);
	mutex_exit(&devevent_lock);

	seldestroy(&d->selinfo);
	mutex_destroy(&d->lock);
	kmem_free(d, sizeof(*d));
	cv_destroy(&d->cv);

	DPRINTF(("/dev/xenevt: close done.\n"));

	fp->f_data = NULL;

	return 0;
}

const struct fileops xenevt_dev_fileops = {
        .fo_read = xenevt_dev_read,
        .fo_write = xenevt_dev_write,
        .fo_ioctl = xenevt_dev_ioctl,
        .fo_fcntl = fnullop_fcntl,
        .fo_poll = xenevt_dev_poll,
        .fo_stat = fbadop_stat,
        .fo_close = xenevt_dev_close,
        .fo_kqfilter = fnullop_kqfilter,
        .fo_restart = fnullop_restart,
};

int
xenevt_dev_open(struct file *fp, void **fdata_r)
{
	struct xenevt_dev_data *d;

	d = kmem_zalloc(sizeof(*d), KM_SLEEP);

	mutex_init(&d->lock, MUTEX_DEFAULT, IPL_HIGH);
	selinit(&d->selinfo);
	cv_init(&d->cv, "xenevt");

	*fdata_r = d;
	return 0;
}

void xenevt_dev_init(void)
{
	mutex_init(&devevent_lock, MUTEX_DEFAULT, IPL_NONE);
	minios_events_register_rump_callback(rump_evtdev_callback);
	bmk_sched_create("xenevt", NULL, 0, xenevt_thread_func, NULL,
			 NULL, 0);
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
