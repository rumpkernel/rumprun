/******************************************************************************
 * kernel.c
 * 
 * Assorted crap goes here, including the initial C entry point, jumped at
 * from head.S.
 * 
 * Copyright (c) 2002-2003, K A Fraser & R Neugebauer
 * Copyright (c) 2005, Grzegorz Milos, Intel Research Cambridge
 * Copyright (c) 2006, Robert Kaiser, FH Wiesbaden
 * 
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
#include <mini-os/mm.h>
#include <mini-os/events.h>
#include <mini-os/time.h>
#include <mini-os/types.h>
#include <mini-os/lib.h>
#include <mini-os/xenbus.h>
#include <mini-os/gnttab.h>
#include <mini-os/netfront.h>
#include <mini-os/blkfront.h>
#include <mini-os/fbfront.h>
#include <mini-os/pcifront.h>
#include <xen/features.h>
#include <xen/version.h>

#include <bmk-core/core.h>
#include <bmk-core/printf.h>

uint8_t _minios_xen_features[XENFEAT_NR_SUBMAPS * 32];

void setup_xen_features(void)
{
    xen_feature_info_t fi;
    int i, j;

    for (i = 0; i < XENFEAT_NR_SUBMAPS; i++) 
    {
        fi.submap_idx = i;
        if (HYPERVISOR_xen_version(XENVER_get_features, &fi) < 0)
            break;
        
        for (j=0; j<32; j++)
            _minios_xen_features[i*32+j] = !!(fi.submap & 1<<j);
    }
}

static void
_app_main(void *arg)
{
    start_info_t *si = arg;

#ifdef CONFIG_PCI
    init_pcifront(NULL);
#endif

    app_main(si);
}

void
bmk_platform_halt(const char *panicstring)
{

	if (panicstring)
		minios_printk("PANIC: %s\n", panicstring);
	minios_stop_kernel();
	minios_do_halt(MINIOS_HALT_POWEROFF);
}

void *
bmk_platform_allocpg2(int shift)
{

	return (void *)minios_alloc_pages(shift);
}

void
bmk_platform_freepg2(void *p, int shift)
{

	minios_free_pages(p, shift);
}

void
bmk_platform_block(bmk_time_t until)
{

	block_domain(until);
	minios_force_evtchn_callback();
}

unsigned long
bmk_platform_splhigh(void)
{
	unsigned long x;

	local_irq_save(x);
	return x;
}

void
bmk_platform_splx(unsigned long x)
{

	local_irq_restore(x);
}

/*
 * INITIAL C ENTRY POINT.
 */
void _minios_start_kernel(start_info_t *si)
{

    bmk_printf_init(minios_putc, NULL);
    bmk_core_init(STACK_SIZE_PAGE_ORDER, PAGE_SIZE);

    arch_init(si);
    trap_init();
    bmk_sched_init();

    /* print out some useful information  */
    minios_printk("  start_info: %p(VA)\n", si);
    minios_printk("    nr_pages: 0x%lx\n", si->nr_pages);
    minios_printk("  shared_inf: 0x%08lx(MA)\n", si->shared_info);
    minios_printk("     pt_base: %p(VA)\n", (void *)si->pt_base); 
    minios_printk("nr_pt_frames: 0x%lx\n", si->nr_pt_frames);
    minios_printk("    mfn_list: %p(VA)\n", (void *)si->mfn_list); 
    minios_printk("   mod_start: 0x%lx(VA)\n", si->mod_start);
    minios_printk("     mod_len: %lu\n", si->mod_len); 
    minios_printk("       flags: 0x%x\n", (unsigned int)si->flags);
    minios_printk("    cmd_line: %s\n",  
           si->cmd_line ? (const char *)si->cmd_line : "NULL");

    /* Set up events. */
    init_events();
    
    /* ENABLE EVENT DELIVERY. This is disabled at start of day. */
    __sti();

    arch_print_info();

    setup_xen_features();

    /* Init memory management. */
    init_mm();

    /* Init time and timers. */
    init_time();

    /* Init the console driver. */
    init_console();

    /* Init grant tables */
    init_gnttab();
 
    /* Init XenBus */
    init_xenbus();

    /* Init scheduler. */
    bmk_sched_startmain(_app_main, &start_info);
    bmk_platform_halt("unreachable");
}

void minios_stop_kernel(void)
{
    /* TODO: fs import */

    local_irq_disable();

    /* Reset grant tables */
    fini_gnttab();

    /* Reset XenBus */
    fini_xenbus();

    /* Reset timers */
    fini_time();

    /* Reset memory management. */
    fini_mm();

    /* Reset events. */
    fini_events();

    /* Reset traps */
    trap_fini();

    /* Reset arch details */
    arch_fini();
}

/*
 * minios_do_halt: Called by "application" code to halt the Xen domain.
 */

void minios_do_halt(int reason)
{
    minios_printk("minios: halting, reason=%d\n", reason);
    for( ;; )
    {
        struct sched_shutdown sched_shutdown = {
            .reason = (reason == MINIOS_HALT_POWEROFF) ?
                SHUTDOWN_poweroff : SHUTDOWN_crash };
        HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
    }
}

/*
 * do_exit: This is called whenever an IRET fails in entry.S.
 * This will generally be because an application has got itself into
 * a really bad state (probably a bad CS or SS). It must be killed.
 * Of course, minimal OS doesn't have applications :-)
 */

void minios_do_exit(void)
{
    minios_printk("Do_exit called!\n");
    stack_walk();
    for( ;; )
    {
        struct sched_shutdown sched_shutdown = { .reason = SHUTDOWN_crash };
        HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
    }
}
