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

#include <bmk/kernel.h>

#include <bmk-core/printf.h>
#include <bmk-core/sched.h>

int bmk_spldepth = 1;

/*
 * amd64 MD descriptors, assimilated from NetBSD
 */

struct region_descriptor {
	unsigned short rd_limit;
	unsigned long rd_base;
} __attribute__((__packed__));

struct gate_descriptor {
	unsigned long gd_looffset:16;	/* gate offset (lsb) */
	unsigned long gd_selector:16;	/* gate segment selector */
	unsigned long gd_ist:3;		/* IST select */
	unsigned long gd_xx1:5;		/* reserved */
	unsigned long gd_type:5;	/* segment type */
	unsigned long gd_dpl:2;		/* segment descriptor priority level */
	unsigned long gd_p:1;		/* segment descriptor present */
	unsigned long gd_hioffset:48;	/* gate offset (msb) */
	unsigned long gd_xx2:8;		/* reserved */
	unsigned long gd_zero:5;	/* must be zero */
	unsigned long gd_xx3:19;	/* reserved */
} __attribute__((__packed__));

struct taskgate_descriptor {
	unsigned long td_lolimit:16;	/* segment extent (lsb) */
	unsigned long td_lobase:24;	/* segment base address (lsb) */
	unsigned long td_type:5;	/* segment type */
	unsigned long td_dpl:2;		/* segment descriptor priority level */
	unsigned long td_p:1;		/* segment descriptor present */
	unsigned long td_hilimit:4;	/* segment extent (msb) */
	unsigned long td_xx1:3;		/* avl, long and def32 (not used) */
	unsigned long td_gran:1;	/* limit granularity (byte/page) */
	unsigned long td_hibase:40;	/* segment base address (msb) */
	unsigned long td_xx2:8;		/* reserved */
	unsigned long td_zero:5;	/* must be zero */
	unsigned long td_xx3:19;	/* reserved */
} __packed;

struct tss {
	unsigned int	tss_reserved1;
	unsigned long	tss_rsp0;
	unsigned long	tss_rsp1;
	unsigned long	tss_rsp2;
	unsigned long	tss_reserved2;
	unsigned long	tss_ist[7];
	unsigned long	tss_reserved3;
	unsigned int	tss_iobase;
	unsigned int	tss_reserved4;
} __attribute__((__packed__)) mytss;

static struct gate_descriptor idt[256];

/* interrupt-not-service-routine */
void bmk_cpu_insr(void);

/* trap "handlers" */
void bmk_cpu_trap_0(void);
void bmk_cpu_trap_2(void);
void bmk_cpu_trap_3(void);
void bmk_cpu_trap_4(void);
void bmk_cpu_trap_5(void);
void bmk_cpu_trap_6(void);
void bmk_cpu_trap_7(void);
void bmk_cpu_trap_8(void);
void bmk_cpu_trap_10(void);
void bmk_cpu_trap_11(void);
void bmk_cpu_trap_12(void);
void bmk_cpu_trap_13(void);
void bmk_cpu_trap_14(void);
void bmk_cpu_trap_17(void);

/* actual interrupt service routines */
void bmk_cpu_isr_clock(void);
void bmk_cpu_isr_9(void);
void bmk_cpu_isr_10(void);
void bmk_cpu_isr_11(void);
void bmk_cpu_isr_14(void);
void bmk_cpu_isr_15(void);

extern unsigned long bmk_cpu_gdt64[];

static void
fillgate(struct gate_descriptor *gd, void *fun, int ist)
{

	gd->gd_hioffset = (unsigned long)fun >> 16;
	gd->gd_looffset = (unsigned long)fun & 0xffff;

	gd->gd_selector = 0x8;
	gd->gd_ist = ist;
	gd->gd_type = 14;
	gd->gd_dpl = 0;
	gd->gd_p = 1;

	gd->gd_zero = 0;
	gd->gd_xx1 = 0;
	gd->gd_xx2 = 0;
	gd->gd_xx3 = 0;
}

#define PIC1_CMD	0x20
#define PIC1_DATA	0x21
#define PIC2_CMD	0xa0
#define PIC2_DATA	0xa1
#define ICW1_IC4	0x01	/* we're going to do the fourth write */
#define ICW1_INIT	0x10
#define ICW4_8086	0x01	/* use 8086 mode */

static int pic2mask = 0xff;

static void
initpic(void)
{

	/*
	 * init pic1: cycle is write to cmd followed by 3 writes to data
	 */
	outb(PIC1_CMD, ICW1_INIT | ICW1_IC4);
	outb(PIC1_DATA, 32);	/* interrupts start from 32 in IDT */
	outb(PIC1_DATA, 1<<2);	/* slave is at IRQ2 */
	outb(PIC1_DATA, ICW4_8086);
	outb(PIC1_DATA, 0xff & ~(1<<2));	/* unmask slave IRQ */

	/* do the slave PIC */
	outb(PIC2_CMD, ICW1_INIT | ICW1_IC4);
	outb(PIC2_DATA, 32+8);	/* interrupts start from 40 in IDT */
	outb(PIC2_DATA, 2);	/* interrupt at irq 2 */
	outb(PIC2_DATA, ICW4_8086);
	outb(PIC2_DATA, pic2mask);
}

#define TIMER_CNTR	0x40
#define TIMER_MODE	0x43
#define TIMER_RATEGEN	0x04
#define TIMER_16BIT	0x30
#define TIMER_HZ	1193182
#define HZ		100

static char intrstack[4096];
static char nmistack[4096];
static char dfstack[4096];

void bmk_cpu_ltr(unsigned long id);

/*
 * This routine fills out the interrupt descriptors so that
 * we can handle interrupts without involving a jump to hyperspace.
 */
void
bmk_cpu_init(void)
{
	struct region_descriptor region;
	int i;

	for (i = 0; i < 32; i++) {
		fillgate(&idt[i], bmk_cpu_insr, 0);
	}

#define FILLGATE(n) fillgate(&idt[n], bmk_cpu_trap_##n, 0)
	FILLGATE(0);
	FILLGATE(2);
	FILLGATE(3);
	FILLGATE(4);
	FILLGATE(5);
	FILLGATE(6);
	FILLGATE(7);
	FILLGATE(8);
	FILLGATE(10);
	FILLGATE(11);
	FILLGATE(12);
	FILLGATE(13);
	FILLGATE(14);
	FILLGATE(17);
#undef FILLGATE
	fillgate(&idt[2], bmk_cpu_trap_2, 2);
	fillgate(&idt[8], bmk_cpu_trap_8, 3);

	region.rd_limit = sizeof(idt)-1;
	region.rd_base = (uintptr_t)(void *)idt;
	bmk_cpu_lidt(&region);

	initpic();

	/*
	 * map clock interrupt.
	 * note, it's still disabled in the PIC, we only enable it
	 * during nanohlt
	 */
	fillgate(&idt[32], bmk_cpu_isr_clock, 0);

	/*
	 * fill TSS
	 */
	mytss.tss_ist[0] = (unsigned long)intrstack + sizeof(intrstack)-16;
	mytss.tss_ist[1] = (unsigned long)nmistack + sizeof(nmistack)-16;
	mytss.tss_ist[2] = (unsigned long)dfstack + sizeof(dfstack)-16;

	struct taskgate_descriptor *td = (void *)&bmk_cpu_gdt64[4];
	td->td_lolimit = 0;
	td->td_lobase = 0;
	td->td_type = 0x9;
	td->td_dpl = 0;
	td->td_p = 1;
	td->td_hilimit = 0xf;
	td->td_gran = 0;
	td->td_hibase = 0xffffffffffUL;
	td->td_zero = 0;
	bmk_cpu_ltr(4*8);

	/* initialize the timer to 100Hz */
	outb(TIMER_MODE, TIMER_RATEGEN | TIMER_16BIT);
	outb(TIMER_CNTR, (TIMER_HZ/HZ) & 0xff);
	outb(TIMER_CNTR, (TIMER_HZ/HZ) >> 8);
}

void bmk_cpu_pagefault(void *, void *);
void
bmk_cpu_pagefault(void *addr, void *rip)
{

	bmk_printf("FATAL pagefault at address %p (rip %p)\n", addr, rip);
	hlt();
}

int
bmk_cpu_intr_init(int intr)
{

	/* XXX: too lazy to keep PIC1 state */
	if (intr < 8)
		return BMK_EGENERIC;

#define FILLGATE(n) case n: fillgate(&idt[32+n], bmk_cpu_isr_##n, 0); break;
	switch (intr) {
		FILLGATE(9);
		FILLGATE(10);
		FILLGATE(11);
		FILLGATE(14);
		FILLGATE(15);
	default:
		return BMK_EGENERIC;
	}
#undef FILLGATE

	/* unmask interrupt in PIC */
	pic2mask &= ~(1<<(intr-8));
	outb(PIC2_DATA, pic2mask);

	return 0;
}

void
bmk_cpu_intr_ack(void)
{

	/*
	 * ACK interrupts on PIC
	 */
	__asm__ __volatile(
	    "movb $0x20, %%al\n"
	    "outb %%al, $0xa0\n"
	    "outb %%al, $0x20\n"
	    ::: "al");
}

bmk_time_t
bmk_cpu_clock_now(void)
{
	uint64_t val;
	unsigned long eax, edx;

	/* um um um */
	__asm__ __volatile__("rdtsc" : "=a"(eax), "=d"(edx));
	val = ((uint64_t)edx<<32)|(eax);

	/* just approximate that 1 cycle = 1ns.  "good enuf" for now */
	return val;
}

void
bmk_cpu_nanohlt(void)
{

	/*
	 * Enable clock interrupt and wait for the next whichever interrupt
	 */
	outb(PIC1_DATA, 0xff & ~(1<<2|1<<0));
	hlt();
	outb(PIC1_DATA, 0xff & ~(1<<2));
}

void
bmk_platform_cpu_sched_settls(struct bmk_tcb *next)
{

	__asm__ __volatile("wrmsr" ::
		"c" (0xc0000100),
		"a" ((uint32_t)(next->btcb_tp)),
		"d" ((uint32_t)(next->btcb_tp >> 32))
	);
}
