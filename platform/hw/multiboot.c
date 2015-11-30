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

#include <hw/types.h>
#include <hw/multiboot.h>
#include <hw/kernel.h>

#include <bmk-core/core.h>
#include <bmk-core/pgalloc.h>
#include <bmk-core/printf.h>
#include <bmk-core/string.h>

#include <bmk-pcpu/pcpu.h>

#define MEMSTART 0x100000

static int
parsemem(uint32_t addr, uint32_t len)
{
	struct multiboot_mmap_entry *mbm;
	unsigned long osend;
	extern char _end[];
	uint32_t off;

	/*
	 * Look for our memory.  We assume it's just in one chunk
	 * starting at MEMSTART.
	 */
	for (off = 0; off < len; off += mbm->size + sizeof(mbm->size)) {
		mbm = (void *)(uintptr_t)(addr + off);
		if (mbm->addr == MEMSTART
		    && mbm->type == MULTIBOOT_MEMORY_AVAILABLE) {
			break;
		}
	}
	if (!(off < len))
		bmk_platform_halt("multiboot memory chunk not found");

	osend = bmk_round_page((unsigned long)_end);
	bmk_assert(osend > mbm->addr && osend < mbm->addr + mbm->len);

	bmk_pgalloc_loadmem(osend, mbm->addr + mbm->len);

	bmk_memsize = mbm->addr + mbm->len - osend;

	return 0;
}

char multiboot_cmdline[BMK_MULTIBOOT_CMDLINE_SIZE];

void
multiboot(struct multiboot_info *mbi)
{
	unsigned long cmdlinelen;
	char *cmdline = NULL,
	     *mbm_name;
	struct multiboot_mod_list *mbm;

	bmk_core_init(BMK_THREAD_STACK_PAGE_ORDER);

	/*
	 * First (and for now, only) multiboot module loaded is used as a
	 * preferred source for configuration (currently overloaded to
	 * `cmdline').
	 * TODO: Split concept of `cmdline' and `config'.
	 */
	if (mbi->flags & MULTIBOOT_INFO_MODS &&
			mbi->mods_count >= 1 &&
			mbi->mods_addr != 0) {
		mbm = (struct multiboot_mod_list *)(uintptr_t)mbi->mods_addr;
		mbm_name = (char *)(uintptr_t)mbm[0].cmdline;
		cmdline = (char *)(uintptr_t)mbm[0].mod_start;
		cmdlinelen =
			mbm[0].mod_end - mbm[0].mod_start;
		if (cmdlinelen >= (BMK_MULTIBOOT_CMDLINE_SIZE - 1))
			bmk_platform_halt("command line too long, "
			    "increase BMK_MULTIBOOT_CMDLINE_SIZE");

		bmk_printf("multiboot: Using configuration from %s\n",
			mbm_name ? mbm_name : "(unnamed module)");
		bmk_memcpy(multiboot_cmdline, cmdline, cmdlinelen);
		multiboot_cmdline[cmdlinelen] = 0;
	}

	/* If not using multiboot module for config, save the command line
	 * before something overwrites it */
	if (cmdline == NULL && mbi->flags & MULTIBOOT_INFO_CMDLINE) {
		cmdline = (char *)(uintptr_t)mbi->cmdline;
		cmdlinelen = bmk_strlen(cmdline);
		if (cmdlinelen >= BMK_MULTIBOOT_CMDLINE_SIZE)
			bmk_platform_halt("command line too long, "
			    "increase BMK_MULTIBOOT_CMDLINE_SIZE");
		bmk_strcpy(multiboot_cmdline, cmdline);
	}

	/* No configuration/cmdline found */
	if (cmdline == NULL)
		multiboot_cmdline[0] = 0;

	if ((mbi->flags & MULTIBOOT_INFO_MEMORY) == 0)
		bmk_platform_halt("multiboot memory info not available\n");

	if (parsemem(mbi->mmap_addr, mbi->mmap_length) != 0)
		bmk_platform_halt("multiboot memory parse failed");

	intr_init();
}
