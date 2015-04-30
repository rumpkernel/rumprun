/*-
 * Copyright (c) 2014 Antti Kantee.  All Rights Reserved.
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

#include <bmk/types.h>
#include <bmk/multiboot.h>
#include <bmk/kernel.h>

#include <bmk-core/core.h>
#include <bmk-core/printf.h>
#include <bmk-core/string.h>

#define MEMSTART 0x100000

static int
parsemem(uint32_t addr, uint32_t len)
{
	struct multiboot_mmap_entry *mbm;
	unsigned long memsize;
	unsigned long ossize, osbegin, osend;
	extern char _end[], _begin[];
	uint32_t off;

	/*
	 * Look for our memory.  We assume it's just in one chunk
	 * starting at MEMSTART.
	 */
	for (off = 0; off < len; off += mbm->size + sizeof(mbm->size)) {
		mbm = (void *)(addr + off);
		if (mbm->addr == MEMSTART
		    && mbm->type == MULTIBOOT_MEMORY_AVAILABLE) {
			break;
		}
	}
	bmk_assert(off < len);

	memsize = mbm->len;
	osbegin = (unsigned long)_begin;
	osend = round_page((unsigned long)_end);
	ossize = osend - osbegin;

	bmk_membase = mbm->addr + ossize;
	bmk_memsize = memsize - ossize;

	bmk_assert((bmk_membase & (PAGE_SIZE-1)) == 0);

	return 0;
}

char bmk_multiboot_cmdline[BMK_MULTIBOOT_CMDLINE_SIZE];

void
bmk_multiboot(struct multiboot_info *mbi)
{
	unsigned long cmdlinelen;

	bmk_printf_init(bmk_cons_putc, NULL);
	bmk_core_init(BMK_THREAD_STACK_PAGE_ORDER, PAGE_SIZE);

	bmk_printf("rump kernel bare metal multibootstrap\n\n");

	/* save the command line before something overwrites it */
	cmdlinelen = bmk_strlen((char *)mbi->cmdline);
	if (cmdlinelen >= BMK_MULTIBOOT_CMDLINE_SIZE)
		bmk_platform_halt("command line too long, "
		    "increase BMK_MULTIBOOT_CMDLINE_SIZE");
	bmk_strcpy(bmk_multiboot_cmdline, (char *)mbi->cmdline);

	if ((mbi->flags & MULTIBOOT_MEMORY_INFO) == 0)
		bmk_platform_halt("multiboot memory info not available\n");

	if (parsemem(mbi->mmap_addr, mbi->mmap_length) != 0)
		bmk_platform_halt("multiboot memory parse failed");

	bmk_isr_init();
}
