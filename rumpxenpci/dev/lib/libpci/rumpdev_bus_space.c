/*-
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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

#include <sys/cdefs.h>

#include <sys/param.h>

#include <dev/pci/pcivar.h>

#include "hyper.h"

int
bus_space_map(bus_space_tag_t bst, bus_addr_t address, bus_size_t size,
	int flags, bus_space_handle_t *handlep)
{
	int rv;

	/*
	 * I/O space we just "låt bli"
 	 *
	 * Memory space needs to be mapped into our guest, so we
	 * make a hypercall to request it.
	 */
	if (bst == 0) {
		*handlep = address;
		rv = 0;
	} else {
		*handlep = (uintptr_t)rumpcomp_pci_map(address, size);
		rv = *handlep ? 0 : EINVAL;
	}
	return rv;
}

uint8_t
bus_space_read_1(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset)
{
	uint8_t rv;

	if (bst == 0) {
		panic("8bit IO space not supported");
	} else {
		rv = *(volatile uint8_t *)(bsh + offset);
	}

	return rv;
}

uint16_t
bus_space_read_2(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset)
{
	uint16_t rv;

	if (bst == 0) {
		panic("16bit IO space not supported");
	} else {
		rv = *(volatile uint16_t *)(bsh + offset);
	}

	return rv;
}

uint32_t
bus_space_read_4(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset)
{
	uint32_t rv;

	if (bst == 0) {
#if 1
		panic("IO space not supported in this build");
#else
		unsigned short addr = bsh + offset;
		__asm__ __volatile__("inl %1, %0" : "=a"(rv) : "d"(addr)); 
#endif
	} else {
		rv = *(volatile uint32_t *)(bsh + offset);
	}

	return rv;
}

void
bus_space_write_1(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, uint8_t v)
{

	if (bst == 0) {
#if 1
		panic("IO space not supported in this build");
#endif
	} else {
		*(volatile uint8_t *)(bsh + offset) = v;
	}
}

void
bus_space_write_2(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, uint16_t v)
{

	if (bst == 0) {
#if 1
		panic("IO space not supported in this build");
#endif
	} else {
		*(volatile uint16_t *)(bsh + offset) = v;
	}
}

void
bus_space_write_4(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, uint32_t v)
{

	if (bst == 0) {
#if 1
		panic("IO space not supported in this build");
#else
		unsigned short addr = bsh + offset;
		__asm__ __volatile__("outl %0, %1" :: "a"(v), "d"(addr));
#endif
	} else {
		*(volatile uint32_t *)(bsh + offset) = v;
	}
}

paddr_t
bus_space_mmap(bus_space_tag_t bst, bus_addr_t addr, off_t off,
	int prot, int flags)
{

	panic("%s: unimplemented", __func__);
}

int
bus_space_subregion(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, bus_size_t size, bus_space_handle_t *nhandlep)
{

	panic("%s: unimplemented", __func__);
}

void
bus_space_unmap(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t size)
{

	panic("%s: unimplemented", __func__);
}
