/*-
 * Copyright (c) 2013, 2014 Antti Kantee.  All Rights Reserved.
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

#include <hw/types.h>
#include <hw/kernel.h>

#include <bmk-core/pgalloc.h>

#include <bmk-pcpu/pcpu.h>

#include "pci_user.h"

#define PCI_CONF_ADDR 0xcf8
#define PCI_CONF_DATA 0xcfc

int
rumpcomp_pci_iospace_init(void)
{

	return 0;
}

static uint32_t
makeaddr(unsigned bus, unsigned dev, unsigned fun, int reg)
{

	return (1<<31) | (bus<<16) | (dev <<11) | (fun<<8) | (reg & 0xfc);
}

int
rumpcomp_pci_confread(unsigned bus, unsigned dev, unsigned fun, int reg,
	unsigned int *value)
{
	uint32_t addr;
	unsigned int data;

	addr = makeaddr(bus, dev, fun, reg);
	outl(PCI_CONF_ADDR, addr);
	data = inl(PCI_CONF_DATA);

	*value = data;
	return 0;
}

int
rumpcomp_pci_confwrite(unsigned bus, unsigned dev, unsigned fun, int reg,
	unsigned int value)
{
	uint32_t addr;

	addr = makeaddr(bus, dev, fun, reg);
	outl(PCI_CONF_ADDR, addr);
	outl(PCI_CONF_DATA, value);

	return 0;
}

static int intrs[BMK_MAXINTR];

int
rumpcomp_pci_irq_map(unsigned bus, unsigned device, unsigned fun,
	int intrline, unsigned cookie)
{

	if (cookie > BMK_MAXINTR)
		return BMK_EGENERIC;

	intrs[cookie] = intrline;
	return 0;
}

void *
rumpcomp_pci_irq_establish(unsigned cookie, int (*handler)(void *), void *data)
{

	if (bmk_isr_init(handler, data, intrs[cookie]) == 0)
		return &intrs[cookie];
	else
		return NULL;
}

/*
 * Well at least there's some benefit to running on physical memory.
 * This stuff is really trivial.
 */

void *
rumpcomp_pci_map(unsigned long addr, unsigned long len)
{

	return (void *)addr;
}

int
rumpcomp_pci_dmalloc(size_t size, size_t align,
	unsigned long *pap, unsigned long *vap)
{
	void *mem;
	int i;

        for (i = 0; size >> (i + BMK_PCPU_PAGE_SHIFT); i++)
                continue;

	mem = bmk_pgalloc(i);
	if (!mem)
		return BMK_ENOMEM;

	*pap = (unsigned long)mem;
	*vap = (unsigned long)mem;

	return 0;
}

int
rumpcomp_pci_dmamem_map(struct rumpcomp_pci_dmaseg *dss, size_t nseg,
	size_t totlen, void **vap)
{

	if (nseg > 1)
		return 1;

	*vap = (void *)dss[0].ds_vacookie;
	return 0;
}

void
rumpcomp_pci_dmafree(unsigned long mem, size_t size)
{
	int i;

        for (i = 0; size >> (i + BMK_PCPU_PAGE_SHIFT); i++)
                continue;
	bmk_pgfree((void *)mem, i);
}

unsigned long
rumpcomp_pci_virt_to_mach(void *virt)
{

	return (unsigned long)virt;
}
