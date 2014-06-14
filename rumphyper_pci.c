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

#include <mini-os/os.h>
#include <mini-os/ioremap.h>
#include <mini-os/pcifront.h>
#include <mini-os/events.h>
#include <mini-os/mm.h>
#include <mini-os/hypervisor.h>

#include "rumpsrc/sys/rump/dev/lib/libpci/pci_user.h" /* XXX */

#include <errno.h>
#include <stdlib.h> /* for malloc */

void *
rumpcomp_pci_map(unsigned long addr, unsigned long len)
{

	return ioremap_nocache(addr, len);
}

int
rumpcomp_pci_confread(unsigned bus, unsigned dev, unsigned fun,
	int reg, unsigned int *rv)
{

	return pcifront_conf_read(NULL, 0, bus, dev, fun, reg, 4, rv);
}

int
rumpcomp_pci_confwrite(unsigned bus, unsigned dev, unsigned fun,
	int reg, unsigned int v)
{

	return pcifront_conf_write(NULL, 0, bus, dev, fun, reg, 4, v);
}

struct ihandler {
	int (*i_handler)(void *);
	void *i_data;
	evtchn_port_t i_prt;
};

static void
hyperhandler(evtchn_port_t prt, struct pt_regs *regs, void *data)
{
	struct ihandler *ihan = data;

	/* XXX: not correct, might not even have rump kernel context now */
	ihan->i_handler(ihan->i_data);
}

/* XXXXX */
static int myintr;
static unsigned mycookie;

int
rumpcomp_pci_irq_map(unsigned bus, unsigned device, unsigned fun,
	int intrline, unsigned cookie)
{

	/* XXX */
	myintr = intrline;
	mycookie = cookie;

	return 0;
}

void *
rumpcomp_pci_irq_establish(unsigned cookie, int (*handler)(void *), void *data)
{
	struct ihandler *ihan;
	evtchn_port_t prt;
	int pirq;

	if (cookie != mycookie)
		return NULL;
	pirq = myintr;

	ihan = malloc(sizeof(*ihan));
	if (!ihan)
		return NULL;
	ihan->i_handler = handler;
	ihan->i_data = data;

	prt = bind_pirq(pirq, 1, hyperhandler, ihan);
	unmask_evtchn(prt);
	ihan->i_prt = prt;

	return ihan;
}

int
rumpcomp_pci_dmalloc(size_t size, size_t align,
	unsigned long *pap, unsigned long *vap)
{
	unsigned long va;
	int i;

	for (i = 0; size >> (i + PAGE_SHIFT); i++)
		continue;

	va = alloc_contig_pages(i, 0); /* XXX: MD interface */
	*vap = (uintptr_t)va;
	*pap = virt_to_mach(va);

	return 0;
}

int
rumpcomp_pci_dmamem_map(struct rumpcomp_pci_dmaseg *dss, size_t nseg,
	size_t totlen, void **vap)
{

	if (nseg > 1) {
		return ENOTSUP;
	}
	*vap = (void *)dss[0].ds_vacookie;

	return 0;
}

unsigned long
rumpcomp_pci_virt_to_mach(void *virt)
{

	return virt_to_mach(virt);
}
