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
#include <bmk/kernel.h>

#include <bmk-core/sched.h>

/* enter the kernel with interrupts disabled */
int bmk_spldepth = 1;

/*
 * i386 MD descriptors, assimilated from NetBSD sys/arch/i386/include/segments.h
 */

struct region_descriptor {
	unsigned short rd_limit:16;
	unsigned int rd_base:32;
} __attribute__((__packed__));

struct gate_descriptor {
	unsigned gd_looffset:16;
	unsigned gd_selector:16;
	unsigned gd_stkcpy:5;
	unsigned gd_xx:3;
	unsigned gd_type:5;
	unsigned gd_dpl:2;
	unsigned gd_p:1;
	unsigned gd_hioffset:16;
} __attribute__((__packed__));

static struct gate_descriptor idt[256];

/* interrupt-not-service-routine */
void bmk_cpu_insr(void);

/* actual interrupt service routines */
void bmk_cpu_isr_clock(void);
void bmk_cpu_isr_10(void);
void bmk_cpu_isr_11(void);

static void
fillgate(struct gate_descriptor *gd, void *fun)
{

	gd->gd_hioffset = (unsigned long)fun >> 16; 
	gd->gd_looffset = (unsigned long)fun;

	/* i was born lucky */
	gd->gd_selector = 0x8;
	gd->gd_stkcpy = 0;
	gd->gd_xx = 0;
	gd->gd_type = 14;
	gd->gd_dpl = 0;
	gd->gd_p = 1;
}

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

#define SEGMENT_CODE	1
#define SEGMENT_DATA	2
#define SEGMENT_GS	3

#define SEG_BYTEGRAN	0
#define SEG_PAGEGRAN	1

static struct segment_descriptor gdt[4];

static void
fillsegment(struct segment_descriptor *sd, int type, int gran)
{

	sd->sd_lobase = 0;
	sd->sd_hibase = 0;

	sd->sd_lolimit = 0xffff;
	sd->sd_hilimit = 0xf;

	sd->sd_type = type;

	/* i was born luckier */
	sd->sd_dpl = 0;
	sd->sd_p = 1;
	sd->sd_xx = 0;
	sd->sd_def32 = 1;
	sd->sd_gran = gran;
}

static void
adjustgs(uintptr_t p, size_t s)
{
	struct segment_descriptor *sd = &gdt[SEGMENT_GS];

	sd->sd_lobase = p & 0xffffff;
	sd->sd_hibase = (p >> 24) & 0xff;
	sd->sd_lolimit = s;
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

/*
 * This routine fills out the interrupt descriptors so that
 * we can handle interrupts without involving a jump to hyperspace.
 */
void
bmk_cpu_init(void)
{
	struct region_descriptor region;
	int i;

	fillsegment(&gdt[SEGMENT_CODE], SDT_MEMERA, SEG_PAGEGRAN);
	fillsegment(&gdt[SEGMENT_DATA], SDT_MEMRWA, SEG_PAGEGRAN);
	fillsegment(&gdt[SEGMENT_GS], SDT_MEMRWA, SEG_BYTEGRAN);

	region.rd_limit = sizeof(gdt)-1;
	region.rd_base = (unsigned int)(uintptr_t)(void *)gdt;
	bmk_cpu_lgdt(&region);

	region.rd_limit = sizeof(idt)-1;
	region.rd_base = (unsigned int)(uintptr_t)(void *)idt;
	bmk_cpu_lidt(&region);

	/*
	 * Apparently things work fine if we don't handle anything,
	 * so let's not.  (p.s. don't ship this code)
	 */
	for (i = 0; i < 32; i++) {
		fillgate(&idt[i], bmk_cpu_insr);
	}

	initpic();

	/*
	 * map clock interrupt.
	 * note, it's still disabled in the PIC, we only enable it
	 * during nanohlt
	 */
	fillgate(&idt[32], bmk_cpu_isr_clock);

	/* initialize the timer to 100Hz */
	outb(TIMER_MODE, TIMER_RATEGEN | TIMER_16BIT);
	outb(TIMER_CNTR, (TIMER_HZ/HZ) & 0xff);
	outb(TIMER_CNTR, (TIMER_HZ/HZ) >> 8);
}

int
bmk_cpu_intr_init(int intr)
{

	/* XXX: too lazy to keep PIC1 state */
	if (intr < 8)
		return BMK_EGENERIC;

	switch (intr) {
	case 10:
		fillgate(&idt[32+10], bmk_cpu_isr_10);
		break;
	case 11:
		fillgate(&idt[32+11], bmk_cpu_isr_11);
		break;
	default:
		return BMK_EGENERIC;
	}

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
bmk_platform_cpu_sched_switch(struct bmk_tcb *prev, struct bmk_tcb *next)
{

	adjustgs(next->btcb_tp, next->btcb_tpsize);
	bmk__cpu_switch(prev, next);
}
