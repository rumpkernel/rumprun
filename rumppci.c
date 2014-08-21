#include <bmk/types.h>
#include <bmk/kernel.h>

#include "pci_user.h"

#define PCI_CONF_ADDR 0xcf8
#define PCI_CONF_DATA 0xcfc

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
		return EGENERIC;

	intrs[cookie] = intrline;
	return 0;
}

void *
rumpcomp_pci_irq_establish(unsigned cookie, int (*handler)(void *), void *data)
{

	if (bmk_isr_netinit(handler, data, intrs[cookie]) == 0)
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
	void *rv;

	rv = bmk_allocpg(round_page(size) / PAGE_SIZE);
	if (rv == NULL)
		return 1;

	*pap = (unsigned long)rv;
	*vap = (unsigned long)rv;
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

unsigned long
rumpcomp_pci_virt_to_mach(void *virt)
{

	return (unsigned long)virt;
}
