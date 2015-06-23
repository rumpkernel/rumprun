/* 
 ****************************************************************************
 * (C) 2003 - Rolf Neugebauer - Intel Research Cambridge
 * (C) 2005 - Grzegorz Milos - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: mm.c
 *      Author: Rolf Neugebauer (neugebar@dcs.gla.ac.uk)
 *     Changes: Grzegorz Milos
 *              
 *        Date: Aug 2003, chages Aug 2005
 * 
 * Environment: Xen Minimal OS
 * Description: memory management related functions
 *              contains buddy page allocator from Xen.
 *
 ****************************************************************************
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#include <mini-os/os.h>
#include <mini-os/hypervisor.h>
#include <xen/memory.h>
#include <mini-os/mm.h>
#include <mini-os/types.h>
#include <mini-os/lib.h>

#include <bmk-core/platform.h>
#include <bmk-core/pgalloc.h>
#include <bmk-core/string.h>

#ifdef MM_DEBUG
#define DEBUG(_f, _a...) \
    minios_printk("MINI_OS(file=mm.c, line=%d) " _f "\n", __LINE__, ## _a)
#else
#define DEBUG(_f, _a...)    ((void)0)
#endif

int free_physical_pages(xen_pfn_t *mfns, int n)
{
    struct xen_memory_reservation reservation;

    set_xen_guest_handle(reservation.extent_start, mfns);
    reservation.nr_extents = n;
    reservation.extent_order = 0;
    reservation.domid = DOMID_SELF;
    return HYPERVISOR_memory_op(XENMEM_decrease_reservation, &reservation);
}

void init_mm(void)
{
    unsigned long start_pfn, max_pfn;
    void *vastartpage, *vaendpage;

    minios_printk("MM: Init\n");

    arch_init_mm(&start_pfn, &max_pfn);
    /*
     * now we can initialise the page allocator
     */
    vastartpage = to_virt(PFN_PHYS(start_pfn));
    vaendpage = to_virt(PFN_PHYS(max_pfn));
    minios_printk("MM: Initialise page allocator for %lx(%lx)-%lx(%lx)\n",
           (u_long)vastartpage, PFN_PHYS(start_pfn),
           (u_long)vaendpage, PFN_PHYS(max_pfn));
    bmk_pgalloc_loadmem((u_long)vastartpage, (u_long)vaendpage);
    minios_printk("MM: done\n");

    bmk_memsize = PFN_PHYS(max_pfn) - PFN_PHYS(start_pfn);

    arch_init_p2m(max_pfn);
    
    arch_init_demand_mapping_area(max_pfn);
}

void fini_mm(void)
{
}

unsigned long long minios_get_l1prot(void)
{
    return L1_PROT;
}
