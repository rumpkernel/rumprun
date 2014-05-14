#include <sys/cdefs.h>

#include <sys/param.h>

#include <dev/pci/pcivar.h>

#include "hyper.h"

void
pci_attach_hook(device_t parent, device_t self, struct pcibus_attach_args *pba)
{

	/* nada */
}

int
pci_bus_maxdevs(pci_chipset_tag_t pc, int busno)
{

	return 32;
}

pcitag_t
pci_make_tag(pci_chipset_tag_t pc, int bus, int device, int function)
{
	pcitag_t pt;
	int *tag;
	unsigned csr;
	int rv;

	CTASSERT(sizeof(pt) >= sizeof(int));

	/* a "bit" ugly, but keeps us MI */
	tag = (int *)&pt;
	*tag = (bus << 16) | (device << 8) | (function << 0);

	/*
	 * moreXXX: we need to enable the device io/mem space somewhere.
	 * might as well do it here.
	 */
	rv = rumpcomp_pci_confread(bus, device, function,
	    PCI_COMMAND_STATUS_REG, &csr);
	if (rv == 0 && (csr & PCI_COMMAND_MEM_ENABLE) == 0)
		rumpcomp_pci_confwrite(bus, device, function,
		    PCI_COMMAND_STATUS_REG, csr | PCI_COMMAND_MEM_ENABLE);

	return pt;
}

pcireg_t
pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	unsigned int rv;
	int bus, device, fun;

	pci_decompose_tag(pc, tag, &bus, &device, &fun);
	rumpcomp_pci_confread(bus, device, fun, reg, &rv);
	return rv;
}

void
pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t data)
{
	int bus, device, fun;

	pci_decompose_tag(pc, tag, &bus, &device, &fun);
	rumpcomp_pci_confwrite(bus, device, fun, reg, data);
}

void
pci_decompose_tag(pci_chipset_tag_t pc, pcitag_t tag,
	int *bp, int *dp, int *fp)
{
	int *t = (int *)&tag;

	*bp = (*t >> 16) & 0xff;
	*dp = (*t >> 8)  & 0xff;
	*fp = (*t >> 0)  & 0xff;
}

int
pci_intr_map(const struct pci_attach_args *pa, pci_intr_handle_t *ih)
{

	*ih = pa->pa_intrline;
	return 0;
}

const char *
pci_intr_string(pci_chipset_tag_t pc, pci_intr_handle_t ih,
	char *buf, size_t buflen)
{

	snprintf(buf, buflen, "pausebreak");
	return buf;
}

void *
pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih,
	int level, int (*func)(void *), void *arg)
{

	return rumpcomp_pci_irq_establish(ih, func, arg);
}

void
pci_intr_disestablish(pci_chipset_tag_t pc, void *ih)
{

	panic("%s: unimplemented", __func__);
}
