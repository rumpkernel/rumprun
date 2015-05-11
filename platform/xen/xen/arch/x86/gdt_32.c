/*-
 * Copyright (c) 2014, 2015 Antti Kantee.  All Rights Reserved.
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

/*
 * We need to provide a segment for gs for TLS to work, so we need to
 * install our own gdt.  Hence, all this complexity follows.
 */

#include <bmk-core/platform.h>
#include <bmk-core/string.h>

#include <mini-os/hypervisor.h>
#include <mini-os/mm.h>
#include <mini-os/os.h>

#if defined(__i386__)

/*
 * i386 MD desciptor, assimilated from NetBSD sys/arch/i386/include/segments.h
 */

struct segment_descriptor {
        unsigned sd_lolimit:16;
        unsigned sd_lobase:24;
        unsigned sd_type:5;
        unsigned sd_dpl:2;
        unsigned sd_p:1;
        unsigned sd_hilimit:4;
        unsigned sd_xx:2;
        unsigned sd_def32:1;
        unsigned sd_gran:1;
        unsigned sd_hibase:8;
} __attribute__((__packed__));

#define SDT_MEMRWA	19	/* memory read write accessed */
#define SDT_MEMERA	27	/* memory execute read accessed */

#define SEGMENT_CODE	(__KERNEL_CS>>3)
#define SEGMENT_DATA	(__KERNEL_DS>>3)
#define SEGMENT_GS	(__KERNEL_GS>>3)

/*
 * Make sure the gdt takes up exactly 1 page since we need(?) to
 * map it read-only before we pass it up to Xen.
 */
#define GDTCOUNT (PAGE_SIZE/sizeof(struct segment_descriptor))
static struct segment_descriptor gdt[GDTCOUNT]
    __attribute__((aligned(PAGE_SIZE)));
static unsigned long long gs_pa;

static union {
	struct segment_descriptor gs;
	unsigned long long gs_raw;
} gs;

void _minios_entry_load_segmentregs(void);

static void
fillsegment(struct segment_descriptor *sd, int type)
{

	sd->sd_lobase = 0;
	sd->sd_hibase = 0;

	sd->sd_lolimit = 0xffff;
	sd->sd_hilimit = 0xf;

	sd->sd_type = type;

	sd->sd_dpl = 1;
	sd->sd_p = 1;
	sd->sd_xx = 0;
	sd->sd_def32 = 1;
	sd->sd_gran = 1;
}

void
tlsswitch32(unsigned long p)
{

	gs.gs.sd_lobase = p & 0xffffff;
	gs.gs.sd_hibase = (p >> 24) & 0xff;
	if (HYPERVISOR_update_descriptor(gs_pa, gs.gs_raw))
		bmk_platform_halt("updating TLS descriptor failed");

	/* reload gs register to finalize change */
	__asm__ __volatile__("mov %0, %%gs" :: "r"(__KERNEL_GS));
}

void
gdtinit32(void)
{
	pte_t pte;
	unsigned long frames[1];
	int error;

	fillsegment(&gdt[SEGMENT_CODE], SDT_MEMERA);
	fillsegment(&gdt[SEGMENT_DATA], SDT_MEMRWA);
	fillsegment(&gdt[SEGMENT_GS], SDT_MEMRWA);

	pte = __pte((virt_to_mach(&gdt)) | L1_PROT_RO);
	if (HYPERVISOR_update_va_mapping((unsigned long)&gdt, pte, UVMF_INVLPG))
		bmk_platform_halt("make gdt r/o");

	frames[0] = virt_to_mfn(&gdt);
	if ((error = HYPERVISOR_set_gdt(frames, 512) != 0)) {
		bmk_platform_halt("set_gdt");
	}
	_minios_entry_load_segmentregs();

	/* cache things for the TLS switch */
	bmk_memcpy(&gs, &gdt[SEGMENT_GS], sizeof(gs));
	gs_pa = virt_to_mach(&gdt[SEGMENT_GS]);
}
#endif /* __i386__ */
