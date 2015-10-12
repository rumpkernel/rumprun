/*-
 * Copyright (c) 2015 Martin Lucina.  All Rights Reserved.
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

#include <arch/x86/hypervisor.h>

int hypervisor_detect(void)
{
	uint32_t eax, ebx, ecx, edx;

	/*
	 * First check for generic "hypervisor present" bit.
	 */
	x86_cpuid(0x0, &eax, &ebx, &ecx, &edx);
	if (eax >= 0x1) {
		x86_cpuid(0x1, &eax, &ebx, &ecx, &edx);
		if (!(ecx & (1 << 31)))
			return 0;
	}
	else
		return 0;

	/*
	 * CPUID leaf at 0x40000000 returns hypervisor vendor signature.
	 * Source: https://lkml.org/lkml/2008/10/1/246
	 */
	x86_cpuid(0x40000000, &eax, &ebx, &ecx, &edx);
	if (!(eax >= 0x40000000))
		return 0;
	/* Xen: "XenVMMXenVMM" */
	if (ebx == 0x566e6558 && ecx == 0x65584d4d && edx == 0x4d4d566e)
		return HYPERVISOR_XEN;
	/* VMware: "VMwareVMware" */
	else if (ebx == 0x61774d56 && ecx == 0x4d566572 && edx == 0x65726177)
		return HYPERVISOR_VMWARE;
	/* Hyper-V: "Microsoft Hv" */
	else if (ebx == 0x7263694d && ecx == 0x666f736f && edx == 0x76482074)
		return HYPERVISOR_HYPERV;
	/* KVM: "KVMKVMKVM\0\0\0" */
	else if (ebx == 0x4b4d564b && ecx == 0x564b4d56 && edx == 0x0000004d)
		return HYPERVISOR_KVM;

	return 0;
}
