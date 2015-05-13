/*
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

#include <sys/uio.h>

#include <mini-os/os.h>
#include <mini-os/netfront.h>

#include <bmk-core/errno.h>
#include <bmk-core/memalloc.h>
#include <bmk-core/string.h>
#include <bmk-core/sched.h>

#include <bmk-rumpuser/rumpuser.h>

#include <rumpxenif/if_virt.h>
#include <rumpxenif/if_virt_user.h>

/*
 * For now, shovel the packets from the interrupt to a
 * thread context via an intermediate set of buffers.  Need
 * to fix this a bit down the road.
 */
#define MAXPKT 2000
struct onepkt {
	unsigned char pkt_data[MAXPKT];
	int pkt_dlen;
};

#define NBUF 64
struct virtif_user {
	struct netfront_dev *viu_dev;
	struct bmk_thread *viu_rcvr;
	struct bmk_thread *viu_thr;
	struct virtif_sc *viu_vifsc;

	int viu_read;
	int viu_write;
	int viu_dying;
	struct onepkt viu_pkts[NBUF];
};

/* make it easy to not link the networking stack. FIXXXME properly */
void rump_virtif_stub(void); void rump_virtif_stub(void) {}
__weak_alias(rump_virtif_pktdeliver,rump_virtif_stub);

/*
 * Ok, based on how (the unmodified) netfront works, we need to
 * consume the data here.  So store it locally (and revisit some day).
 */
static void
myrecv(struct netfront_dev *dev, unsigned char *data, int dlen)
{
	struct virtif_user *viu = netfront_get_private(dev);
	int nextw;

	/* TODO: we should be at the correct spl already, assert how? */

	nextw = (viu->viu_write+1) % NBUF;
	/* queue full?  drop packet */
	if (nextw == viu->viu_read) {
		return;
	}

	if (dlen > MAXPKT) {
		minios_printk("myrecv: pkt len %d too big\n", dlen);
		return;
	}

	bmk_memcpy(viu->viu_pkts[viu->viu_write].pkt_data, data, dlen);
	viu->viu_pkts[viu->viu_write].pkt_dlen = dlen;
	viu->viu_write = nextw;

	if (viu->viu_rcvr)
		bmk_sched_wake(viu->viu_rcvr);
}

static void
pusher(void *arg)
{
	struct virtif_user *viu = arg;
	struct iovec iov;
	struct onepkt *mypkt;
	int flags;

	mypkt = bmk_xmalloc(sizeof(*mypkt));

	/* give us a rump kernel context */
	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_lwproc_newlwp(0);
	rumpuser__hyp.hyp_unschedule();

	local_irq_save(flags);
 again:
	while (!viu->viu_dying) {
		while (viu->viu_read == viu->viu_write) {
			viu->viu_rcvr = bmk_current;
			bmk_sched_blockprepare();
			local_irq_restore(flags);
			bmk_sched();
			local_irq_save(flags);
			viu->viu_rcvr = NULL;
			goto again;
		}
		*mypkt = viu->viu_pkts[viu->viu_read];
		local_irq_restore(flags);

		iov.iov_base = mypkt->pkt_data;
		iov.iov_len =  mypkt->pkt_dlen;

		rumpuser__hyp.hyp_schedule();
		rump_virtif_pktdeliver(viu->viu_vifsc, &iov, 1);
		rumpuser__hyp.hyp_unschedule();

		local_irq_save(flags);
		viu->viu_read = (viu->viu_read+1) % NBUF;
	}
	local_irq_restore(flags);
}

int
VIFHYPER_CREATE(int devnum, struct virtif_sc *vif_sc, uint8_t *enaddr,
	struct virtif_user **viup)
{
	struct virtif_user *viu = NULL;
	int rv, nlocks;

	rumpkern_unsched(&nlocks, NULL);

	viu = bmk_memalloc(sizeof(*viu), 0);
	if (viu == NULL) {
		rv = BMK_ENOMEM;
		goto out;
	}
	bmk_memset(viu, 0, sizeof(*viu));
	viu->viu_vifsc = vif_sc;

	viu->viu_dev = netfront_init(NULL, myrecv, enaddr, NULL, viu);
	if (!viu->viu_dev) {
		rv = BMK_EINVAL; /* ? */
		bmk_memfree(viu);
		goto out;
	}

	viu->viu_thr = bmk_sched_create("xenifp",
	    NULL, 1, pusher, viu, NULL, 0);
	if (viu->viu_thr == NULL) {
		minios_printk("fatal thread creation failure\n"); /* XXX */
		minios_do_exit();
	}

	rv = 0;

 out:
	rumpkern_sched(nlocks, NULL);

	*viup = viu;
	return rv;
}

void
VIFHYPER_SEND(struct virtif_user *viu,
	struct iovec *iov, size_t iovlen)
{
	size_t tlen, i;
	int nlocks;
	void *d;
	char *d0;

	rumpkern_unsched(&nlocks, NULL);
	/*
	 * netfront doesn't do scatter-gather, so just simply
	 * copy the data into one lump here.
	 */
	if (iovlen == 1) {
		d = iov->iov_base;
		tlen = iov->iov_len;
	} else {
		for (i = 0, tlen = 0; i < iovlen; i++) {
			tlen += iov[i].iov_len;
		}
		d = d0 = bmk_xmalloc(tlen);
		for (i = 0; i < iovlen; i++) {
			bmk_memcpy(d0, iov[i].iov_base, iov[i].iov_len);
			d0 += iov[i].iov_len;
		}
	}

	netfront_xmit(viu->viu_dev, d, tlen);

	if (iovlen != 1)
		bmk_memfree(d);

	rumpkern_sched(nlocks, NULL);
}

void
VIFHYPER_DYING(struct virtif_user *viu)
{

	viu->viu_dying = 1;
	if (viu->viu_rcvr)
		bmk_sched_wake(viu->viu_rcvr);
}

void
VIFHYPER_DESTROY(struct virtif_user *viu)
{

	ASSERT(viu->viu_dying == 1);

	bmk_sched_join(viu->viu_thr);
	netfront_shutdown(viu->viu_dev);
	bmk_memfree(viu);
}
