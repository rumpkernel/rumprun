/* XXX: shouldn't be here */

void *rumpcomp_pci_map(unsigned long, unsigned long);
int rumpcomp_pci_confread(unsigned, unsigned, unsigned, int, unsigned int *);
int rumpcomp_pci_confwrite(unsigned, unsigned, unsigned, int, unsigned int); 

int rumpcomp_pci_irq_map(unsigned, unsigned, unsigned, int, unsigned);
void *rumpcomp_pci_irq_establish(unsigned, int (*)(void *), void *);

/* XXX: needs work: support boundary-restricted allocations */
int rumpcomp_pci_dmalloc(size_t, size_t, unsigned long *, unsigned long *);

struct rumpcomp_pci_dmaseg {
	unsigned long ds_pa;
	unsigned long ds_len;
	unsigned long ds_vacookie;
};
int rumpcomp_pci_dmamem_map(struct rumpcomp_pci_dmaseg *, size_t, size_t,
			    void **);

unsigned long rumpcomp_pci_virt_to_mach(void *);
