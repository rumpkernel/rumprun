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

#include <bmk-rumpuser/core_types.h>

#include "pci_user.h"

int
rumpcomp_pci_dmalloc(size_t size, size_t align,
	unsigned long *pap, unsigned long *vap)
{
	void *mem;
	int i;

        for (i = 0; size >> (i + BMK_PCPU_PAGE_SHIFT); i++)
                continue;
	if (align < BMK_PCPU_PAGE_SIZE)
		align = BMK_PCPU_PAGE_SIZE;

	mem = bmk_pgalloc_align(i, align);
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
