/******************************************************************************
 * hypervisor.h
 * 
 * Hypervisor handling.
 * 
 *
 * Copyright (c) 2002, K A Fraser
 * Copyright (c) 2005, Grzegorz Milos
 * Updates: Aravindh Puthiyaparambil <aravindh.puthiyaparambil@unisys.com>
 * Updates: Dietmar Hahn <dietmar.hahn@fujitsu-siemens.com> for ia64
 */

#ifndef _MINIOS_HYPERVISOR_H_
#define _MINIOS_HYPERVISOR_H_

#include <mini-os/types.h>
#include <xen/xen.h>
#if defined(__i386__)
#include <mini-os/x86/x86_32/hypercall-x86_32.h>
#elif defined(__x86_64__)
#include <mini-os/x86/x86_64/hypercall-x86_64.h>
#else
#error "Unsupported architecture"
#endif
#include <mini-os/machine/traps.h>

/*
 * a placeholder for the start of day information passed up from the hypervisor
 */
union start_info_union
{
    start_info_t start_info;
    char padding[512];
};
extern union start_info_union _minios_start_info_union;
#define start_info (_minios_start_info_union.start_info)

/* hypervisor.c */
void minios_force_evtchn_callback(void);
void minios_do_hypervisor_callback(struct pt_regs *regs);
void minios_mask_evtchn(uint32_t port);
void minios_unmask_evtchn(uint32_t port);
void minios_clear_evtchn(uint32_t port);
int minios_hypercall(unsigned op, unsigned long a0,
		     unsigned long a1, unsigned long a2,
		     unsigned long a3, unsigned long a4);

extern int _minios_in_hypervisor_callback;

#endif /* __MINIOS_HYPERVISOR_H__ */
