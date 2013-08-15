/*-
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

/*
 * This is a very simple hypercall layer for the rump kernel
 * to plug into DPDK.  Current status: no known bugs.
 */

#include <sys/types.h>
#include <sys/uio.h>

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_config.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include <rump/rumpuser_component.h>

#include "if_virt.h"
#include "if_virt_rumpcomp.h"

/*
 * Configurables.  Adjust these to be appropriate for your system.
 */

/* change blacklist parameters (-b) if necessary */
static const char *ealargs[] = {
	"if_dpdk",
	"-b 00:00:03.0",
	"-c 1",
	"-n 1",
};

/* change PORTID to the one your want to use */
#define IF_PORTID 0

/* change to the init method of your NIC driver */
#ifndef PMD_INIT
#define PMD_INIT rte_igb_pmd_init
#endif

/* Receive packets in bursts of 16 per read */
#define MAX_PKT_BURST 16

/*
 * No (immediate) need to edit below this line
 */

#define MBSIZE	2048
#define MBCACHE	32
#define NMBUF	8192
#define NDESC	256
#define NQUEUE	1

/* these thresholds don't matter at this stage of optimizing */
static const struct rte_eth_rxconf rxconf = {
	.rx_thresh = {
		.pthresh = 1,
		.hthresh = 1,
		.wthresh = 1,
	},
};
static const struct rte_eth_txconf txconf = {
	.tx_thresh = {
		.pthresh = 1,
		.hthresh = 1,
		.wthresh = 1,
	},
};

static struct rte_mempool *mbpool;

struct virtif_user {
	int viu_devnum;
	struct virtif_sc *viu_virtifsc;
	pthread_t viu_rcvpt;

	/* burst receive context */
	struct rte_mbuf *viu_m_pkts[MAX_PKT_BURST];
	int viu_nbufpkts;
	int viu_bufidx;
};

static void
ifwarn(struct virtif_user *viu, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "warning dpdkif%d: ", viu->viu_devnum);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

#define OUT(a) do { ifwarn(viu, a) ; goto out; } while (/*CONSTCOND*/0)
static int
globalinit(struct virtif_user *viu)
{
	int rv;

	if ((rv = rte_eal_init(sizeof(ealargs)/sizeof(ealargs[0]),
	    /*UNCONST*/(void *)(uintptr_t)ealargs)) < 0)
		OUT("eal init\n");

	/* disable mempool cache due to DPDK bug, not thread safe */
	if ((mbpool = rte_mempool_create("mbuf_pool", NMBUF, MBSIZE, 0/*MBCACHE*/,
	    sizeof(struct rte_pktmbuf_pool_private),
	    rte_pktmbuf_pool_init, NULL,
	    rte_pktmbuf_init, NULL, 0, 0)) == NULL) {
		rv = -EINVAL;
		OUT("mbuf pool\n");
	}

	if ((rv = PMD_INIT()) < 0)
		OUT("pmd init\n");
	if ((rv = rte_eal_pci_probe()) < 0)
		OUT("PCI probe\n");
	if ((rv = rte_eth_dev_count()) == 0)
		OUT("no ports\n");
	rv = 0;

 out:
 	return rv;
}

/*
 * Get mbuf off of interface, push it up into the TCP/IP stack.
 * TODO: share TCP/IP stack mbufs with DPDK mbufs to avoid
 * data copy (in rump_virtif_pktdeliver()).
 */
#define STACK_IOV 16
static void
deliverframe(struct virtif_user *viu)
{
	struct rte_mbuf *m, *m0;
	struct iovec iov[STACK_IOV];
	struct iovec *iovp, *iovp0;

	assert(viu->viu_nbufpkts > 0 && viu->viu_bufidx < MAX_PKT_BURST);
	m0 = viu->viu_m_pkts[viu->viu_bufidx];
	assert(m0 != NULL);
	viu->viu_bufidx++;
	viu->viu_nbufpkts--;

	if (m0->pkt.nb_segs > STACK_IOV) {
		iovp = malloc(sizeof(*iovp) * m0->pkt.nb_segs);
		if (iovp == NULL)
			return; /* drop */
	} else {
		iovp = iov;
	}
	iovp0 = iovp;

	for (m = m0; m; m = m->pkt.next, iovp++) {
		iovp->iov_base = rte_pktmbuf_mtod(m, void *);
		iovp->iov_len = rte_pktmbuf_data_len(m);
	}
	rump_virtif_pktdeliver(viu->viu_virtifsc, iovp0, iovp-iovp0);

	rte_pktmbuf_free(m0);
	if (iovp0 != iov)
		free(iovp0);
}

static void *
receiver(void *arg)
{
	struct virtif_user *viu = arg;

	/* step 1: this newly created host thread needs a rump kernel context */
	rumpuser_component_kthread();

	/* step 2: deliver packets until interface is decommissioned */
	for (;;) {
		/* we have cached frames. schedule + deliver */
		if (viu->viu_nbufpkts > 0) {
			rumpuser_component_schedule(NULL);
			while (viu->viu_nbufpkts > 0) {
				deliverframe(viu);
			}
			rumpuser_component_unschedule();
		}
		
		/* none cached.  ok, try to get some */
		if (viu->viu_nbufpkts == 0) {
			viu->viu_nbufpkts = rte_eth_rx_burst(IF_PORTID,
			    0, viu->viu_m_pkts, MAX_PKT_BURST);
			viu->viu_bufidx = 0;
		}
			
		if (viu->viu_nbufpkts == 0) {
			/*
			 * For now, don't ultrabusyloop.
			 * I don't have an overabundance of
			 * spare cores in my vm.
			 */
			usleep(10000);
		}
	}

	assert(0);
	return NULL;
}


int
VIFHYPER_CREATE(int devnum, struct virtif_sc *vif_sc, uint8_t *enaddr,
	struct virtif_user **viup)
{
	struct rte_eth_conf portconf;
	struct rte_eth_link link;
	struct ether_addr ea;
	struct virtif_user *viu;
	int rv = EINVAL; /* XXX: not very accurate ;) */

	viu = malloc(sizeof(*viu));
	memset(viu, 0, sizeof(*viu));
	viu->viu_devnum = devnum;
	viu->viu_virtifsc = vif_sc;

	/* this is here only for simplicity */
	if ((rv = globalinit(viu)) != 0)
		goto out;

	memset(&portconf, 0, sizeof(portconf));
	if ((rv = rte_eth_dev_configure(IF_PORTID,
	    NQUEUE, NQUEUE, &portconf)) < 0)
		OUT("configure device\n");

	if ((rv = rte_eth_rx_queue_setup(IF_PORTID,
	    0, NDESC, 0, &rxconf, mbpool)) <0)
		OUT("rx queue setup\n");

	if ((rv = rte_eth_tx_queue_setup(IF_PORTID, 0, NDESC, 0, &txconf)) < 0)
		OUT("tx queue setup\n");

	if ((rv = rte_eth_dev_start(IF_PORTID)) < 0)
		OUT("device start\n");

	rte_eth_link_get(IF_PORTID, &link);
	if (!link.link_status) {
		ifwarn(viu, "link down");
	}

	rte_eth_promiscuous_enable(IF_PORTID);
	rte_eth_macaddr_get(IF_PORTID, &ea);
	memcpy(enaddr, ea.addr_bytes, ETHER_ADDR_LEN);

	rv = pthread_create(&viu->viu_rcvpt, NULL, receiver, viu);

 out:
	/* XXX: well this isn't much of an unrolling ... */
	if (rv != 0)
		free(viu);
	else
		*viup = viu;
	return rumpuser_component_errtrans(-rv);
}

/*
 * To send, we copy the data from the TCP/IP stack memory into DPDK
 * memory.  TODO: share TCP/IP stack mbufs with DPDK mbufs to avoid
 * data copy.
 */
void
VIFHYPER_SEND(struct virtif_user *viu,
	struct iovec *iov, size_t iovlen)
{
	struct rte_mbuf *m;
	void *dptr;
	unsigned i;

	m = rte_pktmbuf_alloc(mbpool);
	for (i = 0; i < iovlen; i++) {
		dptr = rte_pktmbuf_append(m, iov[i].iov_len);
		if (dptr == NULL) {
			/* log error somehow? */
			rte_pktmbuf_free(m);
			break;
		}
		memcpy(dptr, iov[i].iov_base, iov[i].iov_len);
	}
	rte_eth_tx_burst(IF_PORTID, 0, &m, 1);
}

void
VIFHYPER_DYING(struct virtif_user *viu)
{

	abort();
}

void
VIFHYPER_DESTROY(struct virtif_user *viu)
{

	abort();
}
