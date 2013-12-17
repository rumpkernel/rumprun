void *rumpcomp_pci_map(unsigned long, unsigned long);
int rumpcomp_pci_confread(unsigned, unsigned, unsigned, int, unsigned int *);
int rumpcomp_pci_confwrite(unsigned, unsigned, unsigned, int, unsigned int); 

void *rumpcomp_pci_irq_establish(int, int (*)(void *), void *);

/* XXX: needs work: support boundary-restricted allocations */
int rumpcomp_pci_dmalloc(size_t, size_t, unsigned long *);

void *rumpcomp_pci_mach_to_virt(unsigned long);
unsigned long rumpcomp_pci_virt_to_mach(void *);
